Shader "Analytics/RenderToUVBasic"
{
	Properties
	{
		_MainTex ("Texture", 2D) = "white" {}
	}
	SubShader
	{
		Tags { "RenderType"="Opaque" }
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

			sampler2D _MainTex;
			float4 _MainTex_ST;
			
			v2f vert (appdata v)
			{
				float2 uvclip = float2(v.uv.x, 1.0f - v.uv.y) * float2(2.0f, 2.0f) - float2(1.0f, 1.0f);

				//uvclip.y = - uvclip.y;
				/*if (_ProjectionParams.x < 0)
					uvclip.y = 1 - uvclip.y;*/

				v2f o;
				o.vertex = float4(uvclip.x, uvclip.y, 1.0f, 1.0f);
				//o.vertex = UnityObjectToClipPos(v.vertex);
				o.uv = TRANSFORM_TEX(v.uv, _MainTex);
				return o;
			}
			
			fixed4 frag (v2f i) : SV_Target
			{
				// sample the texture
				float4 col = tex2D(_MainTex, i.uv);
			//return col;
				return float4(1.0f, 0.0f, 1.0f, 1.0f);
			}
			ENDCG
		}
	}
}