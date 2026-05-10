#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D depthMap;
uniform vec2 texelSize;
uniform float dt;           // Base integration time step
uniform mat4 projection;
uniform mat4 invProjection; // Inverse projection matrix

// Convert non-linear window depth [0, 1] to linear physical depth in View-Space
float getViewZ(float d) {
    float z_ndc = d * 2.0 - 1.0;
    vec4 clip = vec4(0.0, 0.0, z_ndc, 1.0);
    vec4 view = invProjection * clip;
    return view.z / view.w;
}

void main() {
    float center_d = texture(depthMap, TexCoords).r;
    
    // Background pixels do not participate in smoothing, pass through unchanged for later pipeline discard
    if (center_d >= 1.0) {
        FragColor = vec4(1.0, 0.0, 0.0, 1.0);
        return;
    }

    // Get linear view-space depth of the center pixel
    float z_center = getViewZ(center_d);

    float stride = 3.0; 
    vec2 offset = texelSize * stride;

    // Sample non-linear depth of surrounding neighbors
    float d_left  = texture(depthMap, TexCoords - vec2(offset.x, 0.0)).r;
    float d_right = texture(depthMap, TexCoords + vec2(offset.x, 0.0)).r;
    float d_up    = texture(depthMap, TexCoords + vec2(0.0, offset.y)).r;
    float d_down  = texture(depthMap, TexCoords - vec2(0.0, offset.y)).r;

    // If neighbor samples background (>=1.0), fall back to center depth.
    // This forces zero derivative at fluid edges, preventing curvature explosion from depth discontinuities
    float z_left  = (d_left  >= 1.0) ? z_center : getViewZ(d_left);
    float z_right = (d_right >= 1.0) ? z_center : getViewZ(d_right);
    float z_up    = (d_up    >= 1.0) ? z_center : getViewZ(d_up);
    float z_down  = (d_down  >= 1.0) ? z_center : getViewZ(d_down);

    // Compute first and second derivatives in real linear physical space
    float z_dx = (z_right - z_left) * 0.5;
    float z_dy = (z_up - z_down) * 0.5;
    float z_dx2 = z_right - 2.0 * z_center + z_left;
    float z_dy2 = z_up - 2.0 * z_center + z_down;

    // Recalculate projection constants based on sampling stride
    float Cx = 2.0 / (1.0 / offset.x * projection[0][0]);
    float Cy = 2.0 / (1.0 / offset.y * projection[1][1]);

    // Calculate denominator D
    float D = Cy*Cy * z_dx*z_dx + Cx*Cx * z_dy*z_dy + Cx*Cx * Cy*Cy * z_center*z_center;
    
    float new_view_z = z_center;

    // Curvature flow iterative calculation
    if (D > 1e-13) {
        float Ex = 0.5 * z_dx * (0.0) - z_dx2 * D;
        float Ey = 0.5 * z_dy * (0.0) - z_dy2 * D;
        float H = (Cy * Ex + Cx * Ey) / pow(D, 1.5);
        
        // Dynamic adaptation based on physical distance to camera
        float dist = abs(z_center); // Absolute distance to camera in meters
        
        // Dynamic time step: smaller dt when closer to camera
        float adaptive_dt = dt * clamp(dist * 0.2, 0.05, 1.0);
        
        // Smaller max physical movement allowed when closer to camera
        float max_step = clamp(dist * 0.01, 0.0005, 0.15); 

        float step = H * adaptive_dt;
        
        // Add single step clamping limit
        step = clamp(step, -max_step, max_step); 
        
        new_view_z -= step; 
    }

    // Convert smoothed View-Space linear depth back to [0, 1] non-linear depth and write to Framebuffer
    vec4 new_clip = projection * vec4(0.0, 0.0, new_view_z, 1.0);
    float new_ndc = new_clip.z / new_clip.w;
    float new_d = (new_ndc + 1.0) * 0.5;

    FragColor = vec4(new_d, 0.0, 0.0, 1.0);
}