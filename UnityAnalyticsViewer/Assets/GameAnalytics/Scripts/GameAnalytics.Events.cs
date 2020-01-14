using System.IO;
using System.Linq;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;
using StackExchange.Redis;
using Google.Protobuf;
using Google.Protobuf.Reflection;

public class GameAnalyticsEventList
{
    public class EventState
    {
        // todo: timestamp for event?
        public IMessage Message;

        // quickly accessible fields
        public Vector3 Position;
        public float Radius;
        public float Weight;
    }

    public MessageDescriptor EventDescriptor { get; protected set; }
    public FieldDescriptor EventField { get; protected set; }

    private FieldDescriptor _eventRadiusField = null;
    private System.Func<IMessage, float> _getEventRadius = null;
    private System.Func<IMessage, float> _getEventWeight = null;

    public class FieldValue
    {
        public bool Show = true;

        public string DisplayString;
    }

    public class FieldFilterState
    {
        public Dictionary<object, FieldValue> UniqueValues = new Dictionary<object, FieldValue>();
    }

    public Dictionary<FieldDescriptor, FieldFilterState> FieldFilter { get; protected set; }

    // all events of this descriptor type
    public List<EventState> Events { get; protected set; }
    
    public GameAnalyticsEventList(MessageDescriptor descriptor, FieldDescriptor field)
    {
        EventDescriptor = descriptor;
        EventField = field;

        Events = new List<EventState>();
        FieldFilter = new Dictionary<FieldDescriptor, FieldFilterState>();

        Analytics.PointEvent pointEvent;
        if (EventField.TryGetOption<Analytics.PointEvent>(Analytics.AnalyticsExtensions.PointEvent, out pointEvent))
        {
            // parse and cache out the event radius
            float radiusLiteral;
            if (float.TryParse(pointEvent.Radius, out radiusLiteral))
            {
                _getEventRadius = (msg) => { return radiusLiteral; };
            }
            else
            {
                var valueField = EventDescriptor.FindFieldByName(pointEvent.Radius);
                if (valueField != null)
                {
                    _getEventRadius = (msg) => { return System.Convert.ToSingle(valueField.Accessor.GetValue(msg)); };
                }
            }

            // parse and cache out the event weight
            float weightLiteral;
            if (float.TryParse(pointEvent.Weight, out weightLiteral))
            {
                _getEventWeight = (msg) => { return weightLiteral; };
            }
            else
            {
                var valueField = EventDescriptor.FindFieldByName(pointEvent.Weight);
                if (valueField != null)
                {
                    _getEventWeight = (msg) => { return System.Convert.ToSingle(valueField.Accessor.GetValue(msg)); };
                }
            }
        }

        // all fields marked as tracked we will keep track of so they can be used for filtering
        foreach (var fd in EventDescriptor.Fields.InFieldNumberOrder())
        {
            bool b;
            if(fd.TryGetOption<bool>(Analytics.AnalyticsExtensions.TrackEvent, out b) && b)
            {
                var filterState = new FieldFilterState();
                FieldFilter.Add(fd, filterState);
            }
        }

        if (_getEventRadius == null)
            Debug.LogErrorFormat("EventDescriptor {0}.{1} does not define an event radius", EventDescriptor.Name, EventField.Name);
        if (_getEventWeight == null)
            Debug.LogErrorFormat("EventDescriptor {0}.{1} does not define an event weight", EventDescriptor.Name, EventField.Name);
    }

    public void HandleEventChannelInfo(GameAnalytics analytics, RedisChannel channel, RedisValue value)
    {
        IMessage parsedMessage = null;

        bool b;
        if (EventDescriptor.TryGetOption<bool>(Analytics.AnalyticsExtensions.UseJsonEncoding, out b) && b)
        {
            try
            {
                string json = value;

                parsedMessage = JsonParser.Default.Parse(json, EventDescriptor);
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
                using (var instream = CodedInputStream.CreateWithLimits(ms, bytes.Length, 2))
                {
                    try
                    {
                        parsedMessage = EventDescriptor.Parser.ParseFrom(bytes);
                    }
                    catch (System.Exception ex)
                    {
                        Debug.LogErrorFormat("Exception reading message {0}", ex);
                        return;
                    }
                }
            }
        }

        if (parsedMessage != null)
        {
            EventRecieved(analytics, parsedMessage);
        }
    }

    public void EventRecieved(GameAnalytics analytics, IMessage message)
    {
        Vector3 eventPosition = Vector3.zero;
        float eventRadius = _getEventRadius(message);
        float eventWeight = _getEventWeight(message);

        Analytics.PointEvent pointEvent;
        if (EventField.TryGetOption<Analytics.PointEvent>(Analytics.AnalyticsExtensions.PointEvent, out pointEvent))
        {
            Analytics.Pos3 position = EventField.Accessor.GetValue(message) as Analytics.Pos3;
            if (position == null)
                return;

            eventPosition.x = position.X;
            eventPosition.y = position.Y;
            eventPosition.z = position.Z;
        }

        lock (Events)
        {
            Events.Add(new EventState
            {
                Message = message,
                Position = eventPosition,
                Radius = eventRadius,
                Weight = eventWeight
            });
        }

        // keep track of the set of unique values for the tracked fields for filtering
        lock(FieldFilter)
        {
            foreach (var filter in FieldFilter)
            {
                var value = filter.Key.Accessor.GetValue(message);
                if (value != null)
                {
                    if (!filter.Value.UniqueValues.ContainsKey(value))
                    {
                        var displayString = ProtobufUtilities.GetDisplayString(message, filter.Key);

                        filter.Value.UniqueValues.Add(value, new FieldValue() { DisplayString = displayString });

                        analytics.MarkDirty(GameAnalytics.DirtyState.EventUI);
                    }
                }
            }
        }
    }

    // scratch buffers to integrate events in batches
    private const int MaxEventsToIntegrate = 10;
    private Vector4[] _eventPositions = new Vector4[MaxEventsToIntegrate];
    private float[] _eventRadii = new float[MaxEventsToIntegrate];
    private float[] _eventWeights = new float[MaxEventsToIntegrate];

    private void InternalIntegrateEvent(CommandBuffer cmdBuffer, int batchCount, GameAnalyticsMesh mesh, Material integrateEvents)
    {
        // populate the arrays with the events
        cmdBuffer.SetGlobalInt("_EventCount", batchCount);
        cmdBuffer.SetGlobalVectorArray("_EventPosition", _eventPositions);
        cmdBuffer.SetGlobalFloatArray("_EventRadii", _eventRadii);
        cmdBuffer.SetGlobalFloatArray("_EventWeight", _eventWeights);

        mesh.PopulateCommandBuffer(cmdBuffer, integrateEvents);

        //Debug.LogFormat("Attempting to integrate {0} events to {1} to index {2} cbuffsize {3}", batchCount, mesh.name, updateEventIndex, cmdBuffer.sizeInBytes);
    }

    private bool IsEventIncluded(EventState ev, GameAnalyticsMesh gmesh)
    {
        // is this event type relevant to this mesh
        if (gmesh.Overlaps(ev.Position, ev.Radius))
        {
            if (FieldFilter.Count == 0)
                return true;

            // is this event type filtered?
            foreach (var ff in FieldFilter)
            {
                var value = ff.Key.Accessor.GetValue(ev.Message);

                FieldValue fv;
                if (ff.Value.UniqueValues.TryGetValue(value, out fv) && fv.Show)
                {
                    return fv.Show;
                }
            }
        }
        return false;
    }

    public void IntegrateEvents(CommandBuffer cmdBuffer, List<GameAnalyticsMesh> meshes, Material integrateEvents)
    {
        if (meshes.Count == 0)
            return;
        
        lock (Events)
        {
            lock (FieldFilter)
            {
                // start event iteration for the mesh that is furthest behind
                for (int m = 0; m < meshes.Count; ++m)
                {
                    var gmesh = meshes[m];

                    while (gmesh.LastEventIndex < Events.Count)
                    {
                        int batchCount = 0;
                        int lastEvent = gmesh.LastEventIndex;

                        for (int e = gmesh.LastEventIndex; e < Events.Count; ++e)
                        {
                            lastEvent = e;

                            var es = Events[e];

                            if (IsEventIncluded(es, gmesh))
                            {
                                _eventPositions[batchCount] = es.Position;
                                _eventRadii[batchCount] = es.Radius;
                                _eventWeights[batchCount] = es.Weight;

                                ++batchCount;

                                if (batchCount >= MaxEventsToIntegrate)
                                    break;
                            }
                        }

                        if (batchCount > 0)
                        {
                            InternalIntegrateEvent(cmdBuffer, batchCount, gmesh, integrateEvents);
                        }

                        gmesh.MarkEventIndex(lastEvent + 1);
                    }

                    //if (cmdBuffer.sizeInBytes > 200000)
                    //    break;
                }
            }
        }
    }
}

public partial class GameAnalytics
{
    public Gradient ColorGradient;
    public Material IntegrateEvents;
    public Vector2 EventRange = Vector3.zero;

    public bool EventUIDirty { get; set; }

    public Dictionary<RedisChannel, GameAnalyticsEventList> EventViewers { get { return _eventViewers; } }

    private Dictionary<RedisChannel, GameAnalyticsEventList> _eventViewers = new Dictionary<RedisChannel, GameAnalyticsEventList>();

    private List<GameAnalyticsMesh> _eventMeshes = new List<GameAnalyticsMesh>();

    public void Register(GameAnalyticsMesh mesh)
    {
        if (!_eventMeshes.Contains(mesh))
            _eventMeshes.Add(mesh);
    }

    public void UnRegister(GameAnalyticsMesh mesh)
    {
        _eventMeshes.Remove(mesh);
    }

    public enum DirtyState
    {
        EventUI,
    }

    public void MarkDirty(params DirtyState[] dirties)
    {
        if(dirties.Contains(DirtyState.EventUI))
        {
            EventUIDirty = true;
        }
    }

    void HandleEventChannelInfo(RedisChannel channel, RedisValue value)
    {
        GameAnalyticsEventList eventList;
        if (_eventViewers.TryGetValue(channel, out eventList))
        {
            eventList.HandleEventChannelInfo(this, channel, value);
        }
    }

    static int SortByIntegratedEvents(GameAnalyticsMesh m0, GameAnalyticsMesh m1)
    {
        return m0.LastEventIndex - m1.LastEventIndex;
    }

    private CommandBuffer _commandBuffer = null;
    private MessageDescriptor _activeEventLayer = null;

    public MessageDescriptor ActiveEventLayer { get { return _activeEventLayer; } }
    public bool EventRangeDirty { get; set; }

    public void SetActiveEventLayer(MessageDescriptor descriptor)
    {
        if (_activeEventLayer != descriptor)
        {
            _activeEventLayer = descriptor;

            if(_activeEventLayer != null)
            {
                Debug.LogFormat("Active Event Layer set to {0}", _activeEventLayer.Name);
            }

            ResetEventTextures();
        }
    }

    public void InternalIntegrateEvents()
    {
        if (_commandBuffer == null)
        {
            _commandBuffer = new CommandBuffer();
            _commandBuffer.name = "IntegrateEvents";
        }

        _commandBuffer.Clear();

        // sort all the meshes based smallest to largest integrated event count
        _eventMeshes.Sort(SortByIntegratedEvents);

        using (ProfileBlock pb = new ProfileBlock("IntegrateEvents(build)"))
        {
            foreach (var kv in _eventViewers)
            {
                if(kv.Value.EventDescriptor == ActiveEventLayer)
                {
                    kv.Value.IntegrateEvents(_commandBuffer, _eventMeshes, IntegrateEvents);
                }
            }
        }

        using (ProfileBlock pb = new ProfileBlock("IntegrateEvents(execute)"))
        {
            if (_commandBuffer.sizeInBytes > 0)
            {
                Graphics.ExecuteCommandBuffer(_commandBuffer);
            }
        }
    }

    public void ResetEventTextures()
    {
        using (ProfileBlock pb = new ProfileBlock("ResetEventTextures"))
        {
            for (int m = 0; m < _eventMeshes.Count; ++m)
            {
                var gmesh = _eventMeshes[m];
                gmesh.ResetEventTexture();
            }

            EventRangeDirty = true;
        }
    }

    private void InternalPostAutoSubscribe()
    {
        
    }

    private void LateUpdate()
    {
        if (EventRangeDirty)
        {
            float? eventMin = null;
            float? eventMax = null;

            for (int i = 0; i < _eventMeshes.Count; ++i)
            {
                if (eventMin == null || eventMin.Value < _eventMeshes[i].EventRange.x)
                    eventMin = _eventMeshes[i].EventRange.x;

                if (eventMax == null || eventMax.Value < _eventMeshes[i].EventRange.y)
                    eventMax = _eventMeshes[i].EventRange.y;
            }

            if (eventMin.HasValue && eventMax.HasValue)
            {
                EventRange = new Vector2(eventMin.Value, eventMax.Value);
            }
            else
            {
                EventRange = Vector2.zero;
            }

            EventRangeDirty = false;
        }

        Shader.SetGlobalVector("_RemapRange", EventRange);
    }

    public void SetColorGradient()
    {
        // set the raw gradient to a vector array
        Vector4[] colorGradient = new Vector4[ColorGradient.colorKeys.Length];
        for (int i = 0; i < ColorGradient.colorKeys.Length; ++i)
        {
            var k = ColorGradient.colorKeys[i];
            colorGradient[i] = new Vector4(k.color.r, k.color.g, k.color.b, k.time);
        }

        Shader.SetGlobalVectorArray("_ColorGradient", colorGradient);
        Shader.SetGlobalInt("_ColorGradientCount", colorGradient.Length);

        Vector4[] alphaGradient = new Vector4[ColorGradient.alphaKeys.Length];
        for (int i = 0; i < ColorGradient.alphaKeys.Length; ++i)
        {
            var k = ColorGradient.alphaKeys[i];
            alphaGradient[i] = new Vector4(k.alpha, k.time, 0.0f, 0.0f);
        }

        Shader.SetGlobalVectorArray("_AlphaGradient", alphaGradient);
        Shader.SetGlobalInt("_AlphaGradientCount", alphaGradient.Length);
    }
}
