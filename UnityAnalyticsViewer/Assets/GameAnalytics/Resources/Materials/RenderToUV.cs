using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;
using UnityEngine.Rendering;

public class RenderToUV : MonoBehaviour
{
    [SerializeField]
    public Material _renderToUvTexture;

    [SerializeField]
    public RenderTexture _renderTexture;

    [SerializeField]
    public bool _dumpMesh;

    private Renderer _renderer;

	// Use this for initialization
	void Start ()
    {
        _renderTexture = new RenderTexture(512, 512, 24, RenderTextureFormat.ARGB32);
        _renderTexture.name = "EventRenderTexture";

        using (CommandBuffer cb = new CommandBuffer())
        {
            cb.SetRenderTarget(_renderTexture);
            cb.ClearRenderTarget(true, true, Color.blue);
            Graphics.ExecuteCommandBuffer(cb);
        }

        _renderer = GetComponent<Renderer>();
        _renderer.material.mainTexture = _renderTexture;

        if(_dumpMesh)
        {
            var mf = GetComponent<MeshFilter>();
            if(mf != null)
            {
                var savePath = string.Format("Assets/MeshDump/{0}.asset", mf.mesh.name);
                AssetDatabase.CreateAsset(mf.mesh, savePath);
            }
        }
    }
	
	// Update is called once per frame
	void Update ()
    {
        if(_renderToUvTexture != null)
        {
            using (CommandBuffer cb = new CommandBuffer())
            {
                cb.SetRenderTarget(_renderTexture);
                cb.DrawRenderer(_renderer, _renderToUvTexture);
                Graphics.ExecuteCommandBuffer(cb);
            }
        }
    }
}
