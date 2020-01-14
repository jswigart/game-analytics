using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;

public class GameAnalyticsMesh : MonoBehaviour
{
    const int TextureSize = 256;

    public Analytics.PrimitiveOptions CachedOptions { get; protected set; }
    public Analytics.Material CachedMaterial { get; protected set; }

    public RenderTexture RenderTexture;
    public RenderTexture RenderTextureRange;

    public Material RangeMaterial { get; protected set; }

    public int LastEventIndex { get; protected set; }
    public Vector2 EventRange { get; protected set; }

    private MeshFilter _filter;
    private MeshRenderer _renderer;
    private Coroutine _eventRangeCoroutine = null;
    
    public GameAnalyticsMesh()
    {
        CachedMaterial = new Analytics.Material();
    }

    public bool Overlaps(Vector3 position, float radius)
    {
        Vector3 closestPt = _renderer.bounds.ClosestPoint(position);
        return (position - closestPt).sqrMagnitude <= (radius * radius);
    }
    
    public void Init(GameAnalytics analytics, bool renderEvents)
    {
        _filter = GetComponent<MeshFilter>();
        _renderer = GetComponent<MeshRenderer>();
        
        if(renderEvents)
        //if(false)
        {
            RenderTexture = new RenderTexture(TextureSize, TextureSize, 24, RenderTextureFormat.RFloat, RenderTextureReadWrite.Linear);
            RenderTexture.name = "EventRenderTexture";
            
            RenderTextureRange = new RenderTexture(1, 1, 24, RenderTextureFormat.ARGBFloat, RenderTextureReadWrite.Linear);
            RenderTextureRange.name = "EventRenderTextureRange";

            //RangeMaterial = Resources.Load<Material>("Materials/ReduceRangeFull");
            RangeMaterial = analytics.ReduceRangeMaterial;

            // initialize the render texture to blank
            ResetEventTexture();

            //Material eventMaterial = Resources.Load<Material>("Materials/RenderEventTextureToGradient2Pass");

            _renderer.material = analytics.RenderEventMaterial;
            _renderer.material.SetTexture("_EventTexture", RenderTexture);
        }

        if (GameAnalytics.Instance != null)
            GameAnalytics.Instance.Register(this);
    }

    private void OnDestroy()
    {
        if(GameAnalytics.Instance != null)
            GameAnalytics.Instance.UnRegister(this);
    }

    public void ResetEventTexture()
    {
        if(RenderTexture != null)
        {
            using (CommandBuffer cb = new CommandBuffer())
            {
                cb.SetRenderTarget(RenderTexture);
                cb.ClearRenderTarget(true, true, Color.black);

                cb.SetRenderTarget(RenderTextureRange);
                cb.ClearRenderTarget(true, true, Color.black);

                Graphics.ExecuteCommandBuffer(cb);
            }

            LastEventIndex = 0;
            EventRange = Vector2.zero;
            StartRangeRefresh();
        }
    }

    public void MarkEventIndex(int eventIndex)
    {
        LastEventIndex = eventIndex;        
    }

    public void PopulateCommandBuffer(CommandBuffer commandBuffer, Material renderEventMaterial)
    {
        if (RenderTexture != null)
        {
            commandBuffer.SetRenderTarget(RenderTexture);
            commandBuffer.SetGlobalTexture("_EventTexture", RenderTexture);
            commandBuffer.DrawRenderer(_renderer, renderEventMaterial);

            StartRangeRefresh();
        }
    }

    public void UpdateData(Analytics.PrimitiveOptions options, Analytics.Material material)
    {
        CachedOptions = options;
        CachedMaterial = material;
    }

    private void StartRangeRefresh()
    {
        if(_eventRangeCoroutine == null)
        {
            _eventRangeCoroutine = StartCoroutine(RefreshEventRange());
        }
    }

    private IEnumerator RefreshEventRange()
    {
        // wait a frame
        yield return null;

        using (ProfileBlock pb = new ProfileBlock("RefreshEventRange(blit)"))
        {
            if(RangeMaterial != null)
            {
                using (CommandBuffer cb = new CommandBuffer())
                {
                    cb.Blit(RenderTexture, RenderTextureRange, RangeMaterial);
                    Graphics.ExecuteCommandBuffer(cb);
                }
            }
        }

        var request = AsyncGPUReadback.Request(RenderTextureRange, 0, TextureFormat.RGBAFloat);

        while(true)
        {
            if (request.hasError)
            {
                Debug.LogErrorFormat("asyncRequest: error - {0}", gameObject.name);
                
                // wait a bit
                yield return new WaitForSecondsRealtime(0.2f);

                request = AsyncGPUReadback.Request(RenderTextureRange, 0, TextureFormat.RGBAFloat);
            }
            else if (request.done)
            {
                _eventRangeCoroutine = null; 

                // todo: try and async this? copy to pooled color arrays and analyze them in a job
                using (ProfileBlock pb = new ProfileBlock("RefreshEventRange(read)"))
                {
                    var colorData = request.GetData<Color>();

                    Vector2 newEventRange = new Vector2(float.MaxValue, float.MinValue);

                    for (int c = 0; c < colorData.Length; ++c)
                    {
                        var color = colorData[c];
                        newEventRange.x = Mathf.Min(newEventRange.x, color.r);
                        newEventRange.y = Mathf.Max(newEventRange.y, color.g);
                    }

                    EventRange = newEventRange;

                    GameAnalytics.Instance.EventRangeDirty = true;
                }
                yield break;
            }
            else
            {
                yield return null;
            }
        }
    }
}
