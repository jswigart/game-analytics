// Upgrade NOTE: replaced 'mul(UNITY_MATRIX_MVP,*)' with 'UnityObjectToClipPos(*)'

Shader "Analytics/ReduceRange"
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
				float dx = _MainTex_TexelSize.x*0.5;
				float minimum = tex2D(_MainTex, i.uv + float2(-dx, -dx));
				float maximum = tex2D(_MainTex, i.uv + float2(-dx,-dx));

				minimum = min(minimum, tex2D(_MainTex, i.uv + float2(dx, -dx)));
				minimum = min(minimum, tex2D(_MainTex, i.uv + float2(-dx, dx)));
				minimum = min(minimum, tex2D(_MainTex, i.uv + float2(dx, dx)));

				maximum = max(maximum, tex2D(_MainTex, i.uv + float2(dx,-dx)));
				maximum = max(maximum, tex2D(_MainTex, i.uv + float2(-dx,dx)));
				maximum = max(maximum, tex2D(_MainTex, i.uv + float2(dx,dx)));

				return float4(minimum, maximum, 1.0f, 1.0f);
			}
			ENDCG
		}
	}
}
