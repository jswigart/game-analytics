using System;
using System.Linq;
using System.IO;
using System.Reflection;
using System.Collections;
using System.Collections.Generic;
using StackExchange.Redis;
using LZ4;
using System.Threading.Tasks;
using Unity.Collections;
using UnityEngine;
using UnityEngine.Profiling;
using UnityEditor;
using Google.Protobuf;

public partial class GameAnalytics : MonoBehaviour
{   
    public string RedisHost = "localhost";
    public int DbIdex = 0;

    public bool AutoConnectToEventStream = true;

    public Material DefaultMaterial;
    public Material NavigationMaterial;
    public Material ReduceRangeMaterial;
    public Material RenderEventMaterial;

    public bool DebugCachedMeshes;
    public bool DebugGameEntityInfo;
    public bool DebugGameMeshData;
    public bool DebugGameNode;

    ConnectionMultiplexer _redis = null;

    void StartRedis()
    {
        // todo:
        if (_redis == null)
        {
            //ConfigurationOptions options = new ConfigurationOptions();
            _redis = ConnectionMultiplexer.Connect(RedisHost);

            _redis.ConnectionFailed += ConnectionFailed;
            _redis.ConnectionRestored += ConnectionRestored;
            _redis.ErrorMessage += RedisErrorMessage;
            _redis.InternalError += RedisInternalError;
            _redis.ConfigurationChanged += RedisConfigurationChanged;
            _redis.ConfigurationChangedBroadcast += RedisConfigurationChangedBroadcast;
            
            StartCoroutine(FetchEventList());
            //StartCoroutine(UpdateEntityList());
        }
    }

    void CloseRedis()
    {
        _redis.Close();
        _redis.Dispose();
        _redis = null;
    }

    IDatabase Db
    {
        get
        {
            return _redis.GetDatabase(DbIdex);
        }
    }

    public void PublishChangesToServer(List<Analytics.EditorChangeValue> changes)
    {
        if(changes.Count > 0)
        {
            Analytics.EditorChanges changeBatch = new Analytics.EditorChanges();
            foreach(var ch in changes)
            {
                changeBatch.Changes.Add(ch);
            }

            string publishKey = string.Format("{0}:EditorChanges", ActiveEventStream);
            if (Db.Publish(publishKey, changeBatch.ToString()) == 0)
            {
                Debug.LogErrorFormat("PublishChangeToServer( {0} ) went to no subscribers", changeBatch.ToString());
            }
        }
    }

    Queue<Action> _mainThreadQueue = new Queue<Action>();

    bool _sortEntities = false;

    public class MeshData
    {
        public struct Submesh
        {
            public Analytics.PrimitiveOptions Options;
            public List<Mesh> meshes;
            public List<LineData> lines;
            public Material material;
            public Analytics.Material GameMaterial;
        }

        public List<Submesh> meshes;
        public DateTime updatedAt;
    }

    public Dictionary<string, MeshData> ModelCache = new Dictionary<string, MeshData>();

    Color32 IntToColor(uint rgba)
    {
        Color32 col = new Color32((byte)((rgba) & 0xFF), (byte)((rgba >> 8) & 0xFF), (byte)((rgba >> 16) & 0xFF), (byte)((rgba >> 24) & 0xFF));
        return col;
    }

    void ProcessGameEntityData(RedisValue value)
    {
        Analytics.GameEntityInfo msg = null;

        bool jsonEncoded;
        if (Analytics.GameEntityInfo.Descriptor.TryGetOption<bool>(Analytics.AnalyticsExtensions.UseJsonEncoding, out jsonEncoded) && jsonEncoded)
        {
            try
            {
                string json = value;

                msg = JsonParser.Default.Parse(json, Analytics.GameEntityInfo.Descriptor) as Analytics.GameEntityInfo;

                if (DebugGameEntityInfo)
                {
                    Debug.LogWarningFormat("Entity {0} updated( {1} bytes )", msg.EntityId, json.Length);
                }
            }
            catch (System.Exception ex)
            {
                Debug.LogErrorFormat("Exception reading message {0}", ex);
                return;
            }
        }
        else
        {
            byte[] bytes = value;
            using (var ms = new MemoryStream(bytes))
            {
                using (var instream = Google.Protobuf.CodedInputStream.CreateWithLimits(ms, bytes.Length, 2))
                {
                    try
                    {
                        msg = Analytics.GameEntityInfo.Parser.ParseFrom(bytes);

                        if (DebugGameEntityInfo)
                        {
                            Debug.LogWarningFormat("Entity {0} updated( {1} bytes )", msg.EntityId, bytes.Length);
                        }
                    }
                    catch (Exception ex)
                    {
                        Debug.LogErrorFormat("Exception reading message {0}", ex);
                    }
                }
            }
        }

        if(msg != null)
        {
            lock (_mainThreadQueue)
            {
                _mainThreadQueue.Enqueue(() =>
                {
                    GameAnalyticsEntity gaEnt;
                    GameAnalyticsNode entityRoot = GetOrCreateNode("entities");
                    if (!EntityNodes.TryGetValue(msg.EntityId, out gaEnt))
                    {
                        _sortEntities = true;

                        string objectName = ProtobufUtilities.ParseObjectName(msg, "<unknown>");

                        gaEnt = new GameObject(objectName, typeof(GameAnalyticsEntity)).GetComponent<GameAnalyticsEntity>();
                        gaEnt.transform.SetParent(entityRoot.transform, false);
                        EntityNodes.Add(msg.EntityId, gaEnt);
                    }

                    gaEnt.UpdateData(msg);
                });
            }
        }
    }

    void ProcessGameEntityDelete(RedisValue value)
    {
        Analytics.GameEntityDeleted msg = null;

        bool jsonEncoded;
        if (Analytics.GameEntityDeleted.Descriptor.TryGetOption<bool>(Analytics.AnalyticsExtensions.UseJsonEncoding, out jsonEncoded) && jsonEncoded)
        {
            try
            {
                string json = value;

                msg = JsonParser.Default.Parse(json, Analytics.GameEntityDeleted.Descriptor) as Analytics.GameEntityDeleted;
            }
            catch (System.Exception ex)
            {
                Debug.LogErrorFormat("Exception reading message {0}", ex);
                return;
            }
        }
        else
        {
            byte[] bytes = value;
            using (var ms = new MemoryStream(bytes))
            {
                using (var instream = Google.Protobuf.CodedInputStream.CreateWithLimits(ms, bytes.Length, 2))
                {
                    try
                    {
                        msg = Analytics.GameEntityDeleted.Parser.ParseFrom(bytes);
                    }
                    catch (Exception ex)
                    {
                        Debug.LogErrorFormat("Exception reading message {0}", ex);
                    }
                }
            }
        }

        if(msg != null)
        {
            lock (_mainThreadQueue)
            {
                _mainThreadQueue.Enqueue(() =>
                {
                    if (DebugGameEntityInfo)
                    {
                        Debug.LogWarningFormat("Entity {0} marked for deletion", msg.EntityId);
                    }

                    GameAnalyticsEntity gaEnt;
                    if (EntityNodes.TryGetValue(msg.EntityId, out gaEnt))
                    {
                        GameObject.Destroy(gaEnt.gameObject);
                        EntityNodes.Remove(msg.EntityId);
                    }
                });
            }
        }
    }

    void ProcessMeshData(RedisValue value)
    {
        byte[] bytes = value;
        using (var ms = new MemoryStream(bytes))
        {
            using (var instream = Google.Protobuf.CodedInputStream.CreateWithLimits(ms, bytes.Length, 2))
            {
                try
                {
                    Analytics.GameMeshData msg = Analytics.GameMeshData.Parser.ParseFrom(bytes);

                    byte[] uncompressed = null;

                    if (msg.CompressionType == Analytics.Compression.FastLz)
                    {
                        byte[] compressed = msg.ModelBytes.ToByteArray();
                        uncompressed = LZ4Codec.Decode(compressed, 0, compressed.Length, (int)msg.ModelBytesUncompressed);
                    }
                    else
                    {
                        uncompressed = msg.ModelBytes.ToByteArray();
                    }

                    var mesh = Analytics.Mesh.Parser.ParseFrom(uncompressed);

                    lock (_mainThreadQueue)
                    {
                        _mainThreadQueue.Enqueue(() =>
                        {
                            CreateMeshesFromData(mesh);

                            if (DebugGameMeshData)
                            {
                                Debug.LogWarningFormat("GameMeshData {0} updated( {1} bytes )", mesh.Name, bytes.Length);
                            }
                        });
                    }
                }
                catch (Exception ex)
                {
                    Debug.LogErrorFormat("Exception reading message {0}", ex);
                }
            }
        }
    }

    void ProcessNodeData(RedisValue value)
    {
        byte[] bytes = value;
        using (var ms = new MemoryStream(bytes))
        {
            using (var instream = Google.Protobuf.CodedInputStream.CreateWithLimits(ms, bytes.Length, 2))
            {
                try
                {
                    Analytics.GameNode msg = Analytics.GameNode.Parser.ParseFrom(bytes);
                    
                    lock (_mainThreadQueue)
                    {
                        _mainThreadQueue.Enqueue(() =>
                        {
                            GameAnalyticsNode gnode = GetOrCreateNode(msg.NodePath);
                            if (gnode != null)
                            {
                                gnode.UpdateNode(this, msg);

                                if (DebugGameNode)
                                {
                                    Debug.LogWarningFormat("GameNode {0} updated( {1} bytes )", msg.NodePath, bytes.Length);
                                }
                            }
                        });
                    }
                }
                catch (Exception ex)
                {
                    Debug.LogErrorFormat("Exception reading message {0}", ex);
                }
            }
        }
    }

    void ProcessGameLogMessage(RedisValue value)
    {
        byte[] bytes = value;
        using (var ms = new MemoryStream(bytes))
        {
            using (var instream = Google.Protobuf.CodedInputStream.CreateWithLimits(ms, bytes.Length, 2))
            {
                try
                {
                    Analytics.GameLogMessage msg = Analytics.GameLogMessage.Parser.ParseFrom(bytes);
                    switch (msg.LogType)
                    {
                        case Analytics.LogType.Log:
                            Debug.Log(msg.LogMessage);
                            break;
                        case Analytics.LogType.Warning:
                            Debug.LogWarning(msg.LogMessage);
                            break;
                        case Analytics.LogType.Error:
                            Debug.LogError(msg.LogMessage);
                            break;
                    }
                }
                catch (Exception ex)
                {
                    Debug.LogErrorFormat("Exception reading message {0}", ex);
                }
            }
        }
    }

    public class PointData
    {
        public Vector3 Position;
        public Color32 Color;
        public float Width;
    }

    public class LineData
    {
        public List<Vector3> Vertices;
        public Color32 Color;
        public float Width;
    }

    public class TriangleData
    {
        public Vector3[] Vertices = new Vector3[3];
        public Color32[] Colors = new Color32[3];
    }

    class MeshDataCollection
    {
        public Analytics.PrimitiveOptions Options;
        public List<PointData> Points = new List<PointData>();
        public List<LineData> Lines = new List<LineData>();
        public List<TriangleData> Triangles = new List<TriangleData>();
    }
    
    void CreateMeshesFromData(Analytics.Mesh mesh)
    {
        MeshData meshData = new MeshData();
        meshData.meshes = new List<MeshData.Submesh>();
        meshData.updatedAt = DateTime.UtcNow;

        // allocate a submesh per material up front
        Dictionary<KeyValuePair<int, int>, MeshDataCollection> meshdatas = new Dictionary<KeyValuePair<int, int>, MeshDataCollection>();

        using (ProfileBlock pb = new ProfileBlock("GenerateMeshDatas"))
        {
            foreach (var primitive in mesh.Primitives)
            {
                var submeshKey = new KeyValuePair<int, int>((int)primitive.MaterialIndex, primitive.Options != null ? (int)primitive.Options.PartIndex : 0);

                MeshDataCollection meshdata;
                if (!meshdatas.TryGetValue(submeshKey, out meshdata))
                {
                    meshdata = new MeshDataCollection();
                    meshdata.Options = primitive.Options;
                    meshdatas[submeshKey] = meshdata;
                }

                if (primitive.Type == Analytics.PrimitiveType.Triangles)
                {
                    for (int i = 2; i < primitive.Vertices.Count; i += 3)
                    {
                        var vert0 = primitive.Vertices[i - 2];
                        var vert1 = primitive.Vertices[i - 1];
                        var vert2 = primitive.Vertices[i - 0];

                        TriangleData tri = new TriangleData();
                        tri.Vertices[0] = new Vector3(vert0.X, vert0.Z, vert0.Y); // swizzled due to different coordinate system
                        tri.Vertices[1] = new Vector3(vert1.X, vert1.Z, vert1.Y); // swizzled due to different coordinate system
                        tri.Vertices[2] = new Vector3(vert2.X, vert2.Z, vert2.Y); // swizzled due to different coordinate system

                        tri.Colors[0] = IntToColor(vert0.Color);
                        tri.Colors[1] = IntToColor(vert1.Color);
                        tri.Colors[2] = IntToColor(vert2.Color);

                        meshdata.Triangles.Add(tri);
                    }
                }
                else if (primitive.Type == Analytics.PrimitiveType.Quad)
                {
                    for (int i = 3; i < primitive.Vertices.Count; i += 4)
                    {
                        var vert0 = primitive.Vertices[i - 3];
                        var vert1 = primitive.Vertices[i - 2];
                        var vert2 = primitive.Vertices[i - 1];
                        var vert3 = primitive.Vertices[i - 0];

                        TriangleData tri1 = new TriangleData();
                        tri1.Vertices[0] = new Vector3(vert0.X, vert0.Z, vert0.Y); // swizzled due to different coordinate system
                        tri1.Vertices[1] = new Vector3(vert1.X, vert1.Z, vert1.Y); // swizzled due to different coordinate system
                        tri1.Vertices[2] = new Vector3(vert2.X, vert2.Z, vert2.Y); // swizzled due to different coordinate system
                        tri1.Colors[0] = IntToColor(vert0.Color);
                        tri1.Colors[1] = IntToColor(vert1.Color);
                        tri1.Colors[2] = IntToColor(vert2.Color);
                        meshdata.Triangles.Add(tri1);

                        TriangleData tri2 = new TriangleData();
                        tri2.Vertices[0] = new Vector3(vert0.X, vert0.Z, vert0.Y); // swizzled due to different coordinate system
                        tri2.Vertices[1] = new Vector3(vert2.X, vert2.Z, vert2.Y); // swizzled due to different coordinate system
                        tri2.Vertices[2] = new Vector3(vert3.X, vert3.Z, vert3.Y); // swizzled due to different coordinate system
                        tri2.Colors[0] = IntToColor(vert0.Color);
                        tri2.Colors[1] = IntToColor(vert1.Color);
                        tri2.Colors[2] = IntToColor(vert2.Color);
                        meshdata.Triangles.Add(tri2);
                    }
                }
            }
        }

        using (ProfileBlock pb = new ProfileBlock("GenerateMeshes"))
        {
            foreach (var mdc in meshdatas)
            {
                var materialIndex = mdc.Key.Key;
                var partIndex = mdc.Key.Value;

                Analytics.Material material = mesh.Materials[materialIndex];

                MeshData.Submesh submesh = new MeshData.Submesh();
                submesh.Options = mdc.Value.Options;
                submesh.meshes = new List<Mesh>();

                // used later for line renderers
                submesh.lines = mdc.Value.Lines;

                // generate as many meshes as we need to hold all the vertices(64k vertex limit)

                List<Vector3> vertices = new List<Vector3>();
                List<int> triangles = new List<int>();
                List<Color> colors = new List<Color>();

                int currentTri = 0;

                while (currentTri < mdc.Value.Triangles.Count)
                {
                    vertices.Clear();
                    triangles.Clear();
                    colors.Clear();

                    // generate the triangle mesh
                    for (; currentTri < mdc.Value.Triangles.Count; ++currentTri)
                    {
                        var tdata = mdc.Value.Triangles[currentTri];

                        Vector3 v0 = tdata.Vertices[0];
                        Vector3 v1 = tdata.Vertices[1];
                        Vector3 v2 = tdata.Vertices[2];

                        triangles.Add(triangles.Count);
                        vertices.Add(v0);
                        triangles.Add(triangles.Count);
                        vertices.Add(v1);
                        triangles.Add(triangles.Count);
                        vertices.Add(v2);
                        colors.Add(tdata.Colors[0]);
                        colors.Add(tdata.Colors[1]);
                        colors.Add(tdata.Colors[2]);

                        if (triangles.Count > 60000)
                            break;
                    }

                    UnwrapParam unwrap;
                    UnwrapParam.SetDefaults(out unwrap);

                    Mesh meshchunk = new Mesh();
                    meshchunk.name = string.Format("{0}_piece({1})_chunk({2})", mesh.Name, partIndex, submesh.meshes.Count);
                    meshchunk.subMeshCount = 1;
                    meshchunk.SetVertices(vertices);
                    meshchunk.SetTriangles(triangles, 0, true);

                    var uvs = material.RenderEvents ? Unwrapping.GeneratePerTriangleUV(meshchunk, unwrap) : null;

                    meshchunk.uv = uvs;
                    //meshchunk.uv2 = uvs;

                    //if (colors.Count > 0)
                    //    meshchunk.SetColors(colors);
                    submesh.material = material.Name == "nav" ? NavigationMaterial : DefaultMaterial; // TEMP
                    submesh.GameMaterial = material;
                    //UnityEditor.Unwrapping.GenerateSecondaryUVSet(meshchunk);
                    meshchunk.RecalculateNormals();
                    meshchunk.UploadMeshData(false);
                    
                    //meshchunk.RecalculateBounds();
                    submesh.meshes.Add(meshchunk);
                    
                    //var savePath = string.Format("Assets/MeshDump/{0}.asset", meshchunk.name);
                    //AssetDatabase.CreateAsset(meshchunk, savePath);
                }

                meshData.meshes.Add(submesh);
            }

            if (DebugCachedMeshes)
            {
                Debug.LogWarningFormat("Cached Mesh Data: {0}", mesh.Name);
            }

            ModelCache[mesh.Name] = meshData;

            // signal any active nodes that they may need to update their mesh
            foreach (var kv in SceneNodes)
            {
                kv.Value.CheckForMeshUpdate(this, mesh.Name, meshData);
            }
        }
    }

    public static Dictionary<string, List<KeyValuePair<string, long>>> EnumMap = new Dictionary<string, List<KeyValuePair<string, long>>>();

    public GameAnalyticsNode GetOrCreateNode(string nodePath)
    {
        GameAnalyticsNode gnode = null;

        // handle creating the parent tree
        int lastNodeEnd = nodePath.LastIndexOf(':');
        if (lastNodeEnd > 0)
        {
            string parentNodePath = nodePath.Substring(0, lastNodeEnd);
            var parentNode = GetOrCreateNode(parentNodePath);
            if (parentNode != null)
            {
                if (!SceneNodes.TryGetValue(nodePath, out gnode))
                {
                    var go = new GameObject(nodePath.Replace(parentNodePath+":",""), typeof(GameAnalyticsNode));
                    go.transform.SetParent(transform, false);
                    gnode = go.GetComponent<GameAnalyticsNode>();

                    SceneNodes.Add(nodePath, gnode);
                }

                if (gnode.transform.parent != parentNode.transform)
                    gnode.transform.SetParent(parentNode.transform, false);
            }
        }
        else
        {
            if (!SceneNodes.TryGetValue(nodePath, out gnode))
            {
                var go = new GameObject(nodePath, typeof(GameAnalyticsNode));
                go.transform.SetParent(transform, false);
                gnode = go.GetComponent<GameAnalyticsNode>();

                SceneNodes.Add(nodePath, gnode);
            }
        }

        return gnode;
    }

    public void ChangeEventStream(string newStreamId)
    {
        foreach (var kv in SceneNodes)
        {
            GameObject.DestroyImmediate(kv.Value.gameObject);
        }
        SceneNodes.Clear();

        ActiveEventStream = newStreamId;

        if (!string.IsNullOrEmpty(ActiveEventStream))
        {
            Debug.LogFormat("Active Event Stream {0}", ActiveEventStream);
            
            _redis.GetSubscriber().UnsubscribeAll();

            ResetEventTextures();

            FetchGameEnums();
            FetchAllGameModels();
            FetchAllGameNodes();
            FetchAllGameEntities();

            AutoSubscribeToEvents();

            SubscribeToMessageType(Analytics.GameMeshData.Descriptor);
            SubscribeToMessageType(Analytics.GameNode.Descriptor);
            SubscribeToMessageType(Analytics.GameEntityInfo.Descriptor);
            SubscribeToMessageType(Analytics.GameEntityDeleted.Descriptor);
            SubscribeToMessageType(Analytics.GameLogMessage.Descriptor);
            //SubscribeToMessageType(Analytics.GameWeaponFired.Descriptor);
        }
    }

    void SubscribeToMessageType(Google.Protobuf.Reflection.MessageDescriptor descriptor)
    {
        // we want notifications of changes to the data, so subscribe to each message path, based on our current event stream
        string subscribeKey = string.Format("{0}:{1}*", ActiveEventStream, descriptor.Name);
        _redis.GetSubscriber().Subscribe(subscribeKey, HandleChannelInfo);
        
        Debug.LogFormat("Subscribed to {0}", subscribeKey);
    }

    void AutoSubscribeToEvents()
    {
        // find all Analytics Message Types
        foreach(var t in Assembly.GetExecutingAssembly().GetTypes())
        {
            if (!t.IsClass || t.Namespace != "Analytics")
                continue;
                        
            var descriptorProperty = t.GetProperty("Descriptor", BindingFlags.Public | BindingFlags.Static | BindingFlags.GetProperty);
            if (descriptorProperty == null)
                continue;

            var descriptor = descriptorProperty.GetGetMethod().Invoke(null, null) as Google.Protobuf.Reflection.MessageDescriptor;
            if (descriptor == null)
                continue;

            foreach (var fd in descriptor.Fields.InDeclarationOrder())
            {
                Analytics.PointEvent pointEvent;
                if (fd.TryGetOption<Analytics.PointEvent>(Analytics.AnalyticsExtensions.PointEvent, out pointEvent))
                {
                    string subscribeKey = string.Format("{0}:{1}", ActiveEventStream, descriptor.Name);

                    GameAnalyticsEventList eventState = new GameAnalyticsEventList(descriptor, fd);
                    EventViewers[subscribeKey] = eventState;

                    // initialize the active event layer to the first event type that is received
                    if (ActiveEventLayer == null)
                    {
                        _activeEventLayer = descriptor;
                    }

                    Analytics.RedisKeyType keytype;
                    if (descriptor.TryGetOption<Analytics.RedisKeyType>(Analytics.AnalyticsExtensions.Rediskeytype, out keytype))
                    {
                        switch (keytype)
                        {
                            case Analytics.RedisKeyType.Hmset:
                                Db.HashGetAllAsync(subscribeKey).ContinueWith((task) =>
                                {
                                    HashEntry[] values = task.Result;
                                    for (int i = 0; i < values.Length; ++i)
                                    {
                                        HandleEventChannelInfo(subscribeKey, values[i].Value);
                                    }
                                });
                                break;
                            case Analytics.RedisKeyType.Rpush:
                                Db.ListRangeAsync(subscribeKey).ContinueWith((task) =>
                                {
                                    RedisValue[] values = task.Result;
                                    for (int i = 0; i < values.Length; ++i)
                                    {
                                        HandleEventChannelInfo(subscribeKey, values[i]);
                                    }
                                });
                                break;
                            case Analytics.RedisKeyType.Set:
                                Db.StringGetAsync(subscribeKey).ContinueWith((task) =>
                                {
                                    RedisValue value = task.Result;
                                    HandleEventChannelInfo(subscribeKey, value);
                                });
                                break;
                        }
                    }
                    
                    _redis.GetSubscriber().Subscribe(subscribeKey, HandleEventChannelInfo);

                    Debug.LogFormat("AutoSubscribeToEvents(point event) Subscribed to {0}", subscribeKey);

                    
                }
            }
        }

        InternalPostAutoSubscribe();
    }

    void HandleChannelInfo(RedisChannel channel, RedisValue value)
    {
        GameAnalyticsEventList eventList;
        if (_eventViewers.TryGetValue(channel, out eventList))
        {
            eventList.HandleEventChannelInfo(this, channel, value);
        }

        // remove the event stream identifier from the channel name before we match it up
        string messageChannel = channel.ToString().Replace(ActiveEventStream + ":", "");

        if (messageChannel.StartsWith(Analytics.GameEntityInfo.Descriptor.Name))
        {
            using (ProfileBlock pb = new ProfileBlock("ProcessGameEntityData"))
            {
                ProcessGameEntityData(value);
            }   
        }
        if (messageChannel.StartsWith(Analytics.GameEntityDeleted.Descriptor.Name))
        {
            using (ProfileBlock pb = new ProfileBlock("ProcessGameEntityDelete"))
            {
                ProcessGameEntityDelete(value);
            }
        }        
        else if (messageChannel.StartsWith(Analytics.GameMeshData.Descriptor.Name))
        {
            using (ProfileBlock pb = new ProfileBlock("ProcessMeshData"))
            {
                ProcessMeshData(value);
            }
        }
        else if (messageChannel.StartsWith(Analytics.GameLogMessage.Descriptor.Name))
        {
            ProcessGameLogMessage(value);
        }
        else if (messageChannel.StartsWith(Analytics.GameNode.Descriptor.Name))
        {
            using (ProfileBlock pb = new ProfileBlock("ProcessNodeData"))
            {
                ProcessNodeData(value);
            }
        }
    }

    void FetchGameEnums()
    {
        // Fetch all enumerations
        HashEntry[] enumHashes = Db.HashGetAll(string.Format("{0}:{1}", ActiveEventStream, Analytics.GameEnum.Descriptor.Name));
        for (int i = 0; i < enumHashes.Length; ++i)
        {
            byte[] bytes = enumHashes[i].Value;
            using (var ms = new MemoryStream(bytes))
            {
                using (var instream = Google.Protobuf.CodedInputStream.CreateWithLimits(ms, bytes.Length, 2))
                {
                    try
                    {
                        Analytics.GameEnum msg = Analytics.GameEnum.Parser.ParseFrom(bytes);

                        List<KeyValuePair<string, long>> evalues = new List<KeyValuePair<string, long>>();
                        foreach (var value in msg.Values)
                        {
                            KeyValuePair<string, long> pair = new KeyValuePair<string, long>(value.Name, value.Value);
                            evalues.Add(pair);
                        }

                        EnumMap[msg.Enumname] = evalues.OrderBy((x) => x.Key).ToList();
                    }
                    catch (Exception ex)
                    {
                        Debug.LogErrorFormat("Exception reading message {0}", ex);
                    }
                }
            }
        }
    }

    void FetchAllGameNodes()
    {
        // fetch initial game models
        string key = string.Format("{0}:{1}", ActiveEventStream, Analytics.GameNode.Descriptor.Name);        
        var task = Db.HashGetAllAsync(key);

        task.ContinueWith((t) =>
        {
            Debug.LogFormat("Fetched {0} Nodes", t.Result.Length);
            for (int i = 0; i < t.Result.Length; ++i)
            {
                ProcessNodeData(t.Result[i].Value);
            }
        });
    }

    void FetchAllGameModels()
    {
        // fetch initial game models
        string key = string.Format("{0}:{1}", ActiveEventStream, Analytics.GameMeshData.Descriptor.Name);
        var task = Db.HashGetAllAsync(key);

        task.ContinueWith((t) =>
        {
            Debug.LogFormat("Fetched {0} GameMeshDatas", t.Result.Length);
            for (int i = 0; i < t.Result.Length; ++i)
            {
                ProcessMeshData(t.Result[i].Value);
            }
        });        
    }

    void FetchAllGameEntities()
    {
        string modelKey = string.Format("{0}:{1}", ActiveEventStream, Analytics.GameEntityInfo.Descriptor.Name);
        var task = Db.HashGetAllAsync(modelKey, CommandFlags.None);

        task.ContinueWith((t) =>
        {
            for (int i = 0; i < task.Result.Length; ++i)
            {
                ProcessGameEntityData(task.Result[i].Value);
            }
        });
    }

    IEnumerator FetchEventList()
    {
        // keep updating the event stream list
        while (true)
        {
            List<string> streamKeys = new List<string>();

            var slist = Db.ListRange("event_streams");
            foreach (var s in slist)
                streamKeys.Add(s.ToString());

            AllEventStreams = streamKeys.ToArray();

            if(AutoConnectToEventStream)
            {
                // if not set, choose the last one
                if (string.IsNullOrEmpty(ActiveEventStream))
                    ChangeEventStream(AllEventStreams.LastOrDefault());
            }

            yield return new WaitForSeconds(2.0f);
        }
    }

    private void ConnectionFailed(object sender, ConnectionFailedEventArgs e)
    {
        if (e.ConnectionType == ConnectionType.Subscription)
        {
            Debug.LogErrorFormat("RedisNotification - ConnectionFailed -- lost subscriptions: {0}, {1}", e.EndPoint.ToString(), e.Exception);
        }
        else if (e.ConnectionType == ConnectionType.Interactive)
        {
            Debug.LogErrorFormat("RedisNotification - ConnectionFailed -- lost Interactive mode: {0}, {1}", e.EndPoint.ToString(), e.Exception);
        }
    }

    void ConnectionRestored(object sender, ConnectionFailedEventArgs e)
    {
        if (e.ConnectionType == ConnectionType.Subscription)
        {
            Debug.LogWarningFormat("RedisNotification - ConnectionRestored -- Resubscribed to subscriptions");
        }
        else if (e.ConnectionType == ConnectionType.Interactive)
        {
            Debug.LogWarningFormat("RedisNotification - ConnectionRestored -- Interactive mode");
        }
    }

    private void RedisErrorMessage(object sender, RedisErrorEventArgs e)
    {
        Debug.LogErrorFormat("RedisError -- {0} : {1}", e.EndPoint.ToString(), e.Message);
    }

    private void RedisInternalError(object sender, InternalErrorEventArgs e)
    {
        if (e.ConnectionType == ConnectionType.Subscription)
        {
            Debug.LogErrorFormat("RedisInternalError -- lost subscriptions: {0}, {1}", e.EndPoint.ToString(), e.Exception);
        }
        else if (e.ConnectionType == ConnectionType.Interactive)
        {
            Debug.LogErrorFormat("RedisInternalError -- lost Interactive mode: {0}, {1}", e.EndPoint.ToString(), e.Exception);
        }
    }

    private void RedisConfigurationChangedBroadcast(object sender, EndPointEventArgs e)
    {
        Debug.LogWarningFormat("RedisNotification - Configuration Changed Broadcast: {0}", e.EndPoint.ToString());
    }

    private void RedisConfigurationChanged(object sender, EndPointEventArgs e)
    {
        Debug.LogWarningFormat("RedisNotification - Configuration Changed {0}", e.EndPoint.ToString());
    }

    private void Update()
    {
        lock (_mainThreadQueue)
        {   
            while (_mainThreadQueue.Count > 0)
            {
                var f = _mainThreadQueue.Dequeue();

                using (ProfileBlock pb = new ProfileBlock(string.Format("Handle({0})", f.Method.Name)))
                {
                    f();
                }
            }
        }

        if (_sortEntities)
        {
            using (ProfileBlock pb = new ProfileBlock("SortEntities"))
            {
                _sortEntities = false;

                var sortedKeys = EntityNodes.Keys.ToList();
                sortedKeys.Sort();

                for (int i = 0; i < sortedKeys.Count; ++i)
                {
                    GameAnalyticsEntity ent;
                    if (EntityNodes.TryGetValue(sortedKeys[i], out ent))
                    {
                        if (ent.transform.GetSiblingIndex() != i)
                            ent.transform.SetSiblingIndex(i);
                    }
                }
            }
        }

        InternalIntegrateEvents();
    }

}
