// Upgrade NOTE: replaced 'mul(UNITY_MATRIX_MVP,*)' with 'UnityObjectToClipPos(*)'

Shader "Analytics/ReduceRangeFull"
{
	Properties
	{
		_MainTex("Texture", 2D) = "white" {}
	}
	SubShader
	{
		Tags { "RenderType" = "Opaque" }
		LOD 100

		Pass
		{
			Cull Off

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

			sampler2D_float _MainTex;
			float4 _MainTex_ST;
			float4 _MainTex_TexelSize;

			v2f vert(appdata v)
			{
				v2f o;
				o.vertex = UnityObjectToClipPos(v.vertex);
				o.uv = TRANSFORM_TEX(v.uv, _MainTex);
				return o;
			}
			float4 frag(v2f i) : SV_Target
			{
				float minimum = 0.0f;
				float maximum = 0.0f;
				float total = 0.0f;

				for (int h = 0; h < _MainTex_TexelSize.w; ++h)
				{
					for (int w = 0; w < _MainTex_TexelSize.z; ++w)
					{
						float value = tex2D(_MainTex, float2(w * _MainTex_TexelSize.x, h * _MainTex_TexelSize.y));


						minimum = min(minimum, value);
						maximum = max(maximum, value);
						total += value;
					}
				}

				float average = total / (_MainTex_TexelSize.w * _MainTex_TexelSize.z);

				return float4(minimum, maximum, average, 1.0f);
			}
			ENDCG
		}
	}
}
