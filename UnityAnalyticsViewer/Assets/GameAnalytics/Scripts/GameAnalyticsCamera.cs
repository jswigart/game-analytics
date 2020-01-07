using UnityEngine;

[ExecuteInEditMode]
public class GameAnalyticsCamera : MonoBehaviour
{
    public Material EffectMaterial;

    void OnEnable ()
    {
        var camera = GetComponent<Camera>();
        camera.depthTextureMode = DepthTextureMode.Depth;		
	}

    private void Update()
    {
    }

    private void OnRenderImage(RenderTexture src, RenderTexture dest)
    {
        if(EffectMaterial != null)
        {
            Graphics.Blit(src, dest, EffectMaterial);
        }
    }
}
