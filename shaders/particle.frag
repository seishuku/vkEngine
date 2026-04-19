#version 450

layout (location=0) in flat vec4 vColor;
layout (location=1) in vec2 vUV;
layout (location=2) in flat vec3 vVelocityView;
layout (location=3) in flat vec3 vPositionView;

//layout (binding=0) uniform sampler2D Particle;

layout (location=0) out vec4 Output;

void main()
{
	vec2 uv = vUV * 2.0 - 1.0;
	
	// View-space velocity vector
	vec3 vel3 = normalize(vVelocityView);
	vec3 viewRay = normalize(vPositionView); // view-space ray, origin is camera
	float forward = abs(dot(vel3, viewRay));
	
	// parameters
	float sparkLength = 0.75;
	float maxWidth = 0.15;
	float blendThreshold = 0.5;
	
	// compute sphere dist for end-on view
	float sphereDist = length(uv) - maxWidth;
	
	// compute spark dist normally
	float sparkDist;
	{
		vec2 velDir = -normalize(vel3.xy);
		float cosA = velDir.x;
		float sinA = velDir.y;
		mat2 rot = mat2(cosA, -sinA, sinA, cosA);
		vec2 rotatedUV = rot * uv;
		float x = clamp(rotatedUV.x, 0.0, sparkLength);
		float width = mix(maxWidth, 0.0, x / sparkLength);
		sparkDist = abs(rotatedUV.y) - width;
		if (rotatedUV.x > sparkLength) {
			vec2 tip = vec2(sparkLength, 0.0);
			sparkDist = length(rotatedUV - tip);
		} else if (rotatedUV.x < 0.0) {
			vec2 start = vec2(0.0, 0.0);
			sparkDist = length(rotatedUV - start) - maxWidth;
		}
	}
	
	// blend between spark and sphere based on forward
	float t = smoothstep(blendThreshold - 0.2, blendThreshold + 0.2, forward);
	// t==0 => spark, t==1 => sphere
	float dist = mix(sparkDist, sphereDist, t);
	
	// Alpha based on SDF
	float alpha = 1.0 - smoothstep(-0.1, 0.1, dist);
	
	if(alpha < 0.001)
		discard;
	
	Output = vec4(vColor.xyz, alpha);
}
