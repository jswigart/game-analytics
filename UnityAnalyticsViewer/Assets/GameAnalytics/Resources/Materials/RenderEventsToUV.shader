Shader "Analytics/RenderEventsToUV"
{
	Properties
	{
	}

	SubShader
	{
		Tags { "RenderType" = "Opaque" }
		LOD 100

		Pass
		{
			Cull Off
			ZTest Always
			ZWrite Off
			Blend One One // additive blend mode ensures the event values will accumulate each iteration

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
				float4 vertex : SV_POSITION;
				float4 worldPos : TEXCOORD2;
			};
			
			uniform int _EventCount = 0;
			uniform float4 _EventPosition[10];
			uniform float _EventRadii[10];
			uniform float _EventWeight[10];

			float3 nearestPointOnLine(float3 start, float3 end, float3 position)
			{
				float3 sp = position - start;
				float3 se = end - start;
				float sedot = dot(se, se);
				float sesp = dot(se, sp);
				if (sedot != 0)
				{
					//Get scalar for point.
					float ratio = clamp(sesp / sedot, 0.0f, 1.0f);
					return start + se * ratio;
				}
				return start;
			}

			v2f vert(appdata v)
			{
				float2 uvclip = float2(v.uv.x, 1.0f - v.uv.y) * float2(2.0f, 2.0f) - float2(1.0f, 1.0f);

				v2f o;
				o.worldPos = mul(unity_ObjectToWorld, v.vertex);
				o.vertex = float4(uvclip.x, uvclip.y, 1.0f, 1.0f);
				return o;
			}

			fixed4 frag(v2f i) : SV_Target
			{
				float newInfluence = 0.0f;

				// calculate the influence for all the events for this pixel
				for (int e = 0; e < _EventCount; ++e)
				{
					float distanceToEvent = distance(i.worldPos, _EventPosition[e]);
					if (distanceToEvent <= _EventRadii[e])
					{
						float d = clamp((_EventRadii[e] - distanceToEvent) / _EventRadii[e], 0.0, 1.0);
						float c = d * _EventWeight[e];
						newInfluence += float4(c, c, c, c);
					}
				}

				return newInfluence;
			}
		ENDCG
	}
	}
}