// Upgrade NOTE: replaced 'mul(UNITY_MATRIX_MVP,*)' with 'UnityObjectToClipPos(*)'

Shader "Analytics/RenderEventTextureToGradient"
{
	Properties
	{
		_EventTexture("Event Texture", 2D) = "white" {}
	}
	SubShader
	{
		Tags
		{
			"Queue" = "Transparent"
			"IgnoreProjector" = "True"
			"RenderType" = "Transparent"
			"PreviewType" = "Plane"
		}
		LOD 100

		Pass
		{
			Cull Off
			ZTest LEqual
			//ZWrite Off
			Blend SrcAlpha OneMinusSrcAlpha
			Lighting Off

			CGPROGRAM
			#pragma vertex vert
			#pragma fragment frag

			#include "UnityCG.cginc"

			struct appdata
			{
				float4 vertex : POSITION;
				float2 uv : TEXCOORD0;
			};

			struct v2f
			{
				float2 uv : TEXCOORD0;
				float4 vertex : SV_POSITION;
			};

			// floating point event texture for the model
			// must be remapped to a color gradient for display
			sampler2D_float _EventTexture;
			float4 _EventTexture_ST;
			float4 _EventTexture_TexelSize;
			
			uniform float4 _ColorGradient[8];
			uniform int _ColorGradientCount = 0;

			uniform float4 _AlphaGradient[8];
			uniform int _AlphaGradientCount = 0;

			// min/max range to remap event values from
			float2 _RemapRange = float2(0, 1);

			v2f vert(appdata v)
			{
				v2f o;
				o.vertex = UnityObjectToClipPos(v.vertex);
				o.uv = TRANSFORM_TEX(v.uv, _EventTexture);
				return o;
			}
			float4 frag(v2f i) : SV_Target
			{
				// sample the event texture
				fixed4 eventColor = tex2D(_EventTexture, i.uv);

				// if there is no event weight for this sample, don't render the pixel
				if (eventColor.r == 0.0f)
				{
					//discard;
					//return float4(0, 0, 0, 0);
				}
				
				// remap the event color to a 0-1 range
				float t = saturate( (eventColor.x - _RemapRange.x) / (_RemapRange.y - _RemapRange.x) );

				float4 c = float4(_ColorGradient[0].r, _ColorGradient[0].g, _ColorGradient[0].b, 1.0f);

				for (int i0 = 1; i0 < _ColorGradientCount; ++i0)
				{
					float3 c0 = _ColorGradient[i0 - 1].rgb;
					float3 c1 = _ColorGradient[i0].rgb;

					float t0 = _ColorGradient[i0 - 1].a;
					float t1 = _ColorGradient[i0].a;
					if (t >= t0 && t <= t1)
					{
						c.rgb = lerp(c0, c1, (t - t0) / (t1 - t0));
						break;
					}
				}

				for (int i1 = 1; i1 < _AlphaGradientCount; ++i1)
				{
					float a0 = _AlphaGradient[i1 - 1].x;
					float a1 = _AlphaGradient[i1].x;

					float t0 = _AlphaGradient[i1 - 1].y;
					float t1 = _AlphaGradient[i1].y;
					if (t >= t0 && t <= t1)
					{
						c.a = lerp(a0, a1, (t - t0) / (t1 - t0));
						break;
					}
				}

				return c;
				//return tex2D(_ColorRampTexture, remapped);
			}
			ENDCG
		}
	}
}
