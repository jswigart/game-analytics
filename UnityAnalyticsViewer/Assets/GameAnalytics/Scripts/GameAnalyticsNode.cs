using System;
using System.Collections.Generic;
using UnityEngine;

public class GameAnalyticsNode : MonoBehaviour
{
    public Analytics.GameNode CachedNode { get; protected set; }

    public GameAnalyticsNode()
    {
        CachedNode = new Analytics.GameNode();
    }
    Dictionary<string, GameObject> MeshNodes = new Dictionary<string, GameObject>();
    
    public void UpdateNode(GameAnalytics analytics, Analytics.GameNode node)
    {
        CachedNode = node;
        
        if (!string.IsNullOrEmpty(node.MeshName))
        {
            GameAnalytics.MeshData meshData;
            if (analytics.ModelCache.TryGetValue(node.MeshName, out meshData))
            {
                UpdateMesh(analytics, node, meshData);
            }
        }

        if (node.EulerRotation != null)
            transform.eulerAngles = new Vector3(node.EulerRotation.Heading, node.EulerRotation.Pitch, node.EulerRotation.Roll);
        if (node.Translation != null)
            transform.position = new Vector3(node.Translation.X, node.Translation.Y, node.Translation.Z);
    }

    DateTime _modelTime = DateTime.UtcNow - TimeSpan.FromDays(1);

    public void CheckForMeshUpdate(GameAnalytics analytics, string meshname, GameAnalytics.MeshData meshdata)
    {
        if(CachedNode.MeshName == meshname)
        {
            UpdateMesh(analytics, CachedNode, meshdata);
        }
    }

    public void UpdateMesh(GameAnalytics analytics, Analytics.GameNode mdlNode, GameAnalytics.MeshData meshdata)
    {
        if (meshdata.updatedAt > _modelTime)
        {
            _modelTime = meshdata.updatedAt;

            // put all submeshes under a mesh node
            GameObject meshGo;
            if (!MeshNodes.TryGetValue(mdlNode.MeshName, out meshGo))
            {
                meshGo = new GameObject("mesh");
                meshGo.transform.SetParent(transform, false);
                MeshNodes.Add(mdlNode.MeshName, meshGo);
            }

            while (meshGo.transform.childCount > 0)
                GameObject.DestroyImmediate(meshGo.transform.GetChild(0).gameObject);
            
            foreach(var submesh in meshdata.meshes)
            {
                if(submesh.lines != null)
                {
                    for(int l = 0; l < submesh.lines.Count; ++l)
                    {
                        var line = submesh.lines[l];

                        GameObject go = new GameObject("lines_"+l, typeof(LineRenderer));
                        go.name = "lines_" + l;
                        go.transform.SetParent(meshGo.transform, false);
                        go.transform.localPosition = Vector3.zero;
                        go.transform.localRotation = Quaternion.identity;
                        go.transform.localScale = Vector3.one;

                        float width = line.Width != 0.0f ? line.Width : 4.0f;

                        var lr = go.GetComponent<LineRenderer>();
                        lr.useWorldSpace = true;
                        lr.alignment = LineAlignment.TransformZ;
                        lr.sharedMaterial = analytics.DefaultMaterial;
                        lr.startColor = line.Color;
                        lr.endColor = line.Color;                        
                        lr.startWidth = width;
                        lr.endWidth = width;
                        lr.generateLightingData = false;
                        lr.positionCount = line.Vertices.Count;
                        lr.SetPositions(line.Vertices.ToArray());
                    }
                }

                // Install the mesh
                if(submesh.meshes != null)
                {
                    foreach(var m in submesh.meshes)
                    {
                        GameObject go = new GameObject(m.name, typeof(MeshFilter), typeof(MeshRenderer), typeof(GameAnalyticsMesh));
                        go.transform.SetParent(meshGo.transform, false);
                        go.transform.localPosition = Vector3.zero;
                        go.transform.localRotation = Quaternion.identity;
                        go.transform.localScale = Vector3.one;

                        var mf = go.GetComponent<MeshFilter>();
                        var mr = go.GetComponent<MeshRenderer>();
                        var gm = go.GetComponent<GameAnalyticsMesh>();

                        //mr.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.Off;
                        //mr.lightProbeUsage = UnityEngine.Rendering.LightProbeUsage.Off;
                        //mr.allowOcclusionWhenDynamic = false;
                        mf.mesh = m;
                        mr.sharedMaterial = submesh.material;
                        
                        gm.Init(analytics, submesh.GameMaterial.RenderEvents);

                        gm.UpdateData(submesh.Options, submesh.GameMaterial);
                    }
                }
            }

            _DrawBoundsExpire = Time.time + 5.0f;
        }
    }
    
    float? _DrawBoundsExpire = 0.0f;

    private void OnDrawGizmos()
    {
        float f = Time.time;

        if(_DrawBoundsExpire.HasValue)
        {
            var mf = GetComponentsInChildren<MeshFilter>();
            for(int i = 0; i < mf.Length; ++i)
            {
                Gizmos.DrawWireMesh(mf[i].sharedMesh, mf[i].transform.position, mf[i].transform.rotation);
            }

            if (Time.time > _DrawBoundsExpire.Value)
                _DrawBoundsExpire = null;
        }

    }
}
