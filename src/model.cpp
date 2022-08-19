void destroyModel(int model)
{
    free(loaded_models[model].vertices);
    free(loaded_models[model].normals);
    free(loaded_models[model].texture_coords);

    if (loaded_models[model].face_type == VERTEX_ALL || loaded_models[model].face_type == VERTEX_ALL_ALPHA) {
        glDeleteTextures(1, &loaded_models[model].texture_id);
    }
}

int loadTexture(const char* filename)
{
    assert(filename);
    int w, h, n;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* pixels = stbi_load(filename, &w, &h, &n, 3);
    assert(pixels && w > 0 && h > 0 && n == 3);

    GLuint tex;
    glGenTextures(1, &tex);

    glBindTexture(GL_TEXTURE_2D, tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
        glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(pixels);

    return tex;
}

void model_add_normal_map(Model* model, const char *normal_map_filename) {
    model->has_normal_map = true;
    model->normal_map_id = loadTexture(normal_map_filename);
}

void generate_vertex_tangents_and_bitangents(Model* model, int vertex_idx) {
    int k = vertex_idx;

	// face vertices
	vec3 A = { model->vertices[k][0], model->vertices[k][1], model->vertices[k][2] };
	vec3 B = { model->vertices[k + 1][0], model->vertices[k + 1][1], model->vertices[k + 1][2] };
	vec3 C = { model->vertices[k + 2][0], model->vertices[k + 2][1], model->vertices[k + 2][2] };

	// face UB
	vec2 UV_A = { model->texture_coords[k][0], model->texture_coords[k][1] };
	vec2 UV_B = { model->texture_coords[k + 1][0], model->texture_coords[k + 1][1] };
	vec2 UV_C = { model->texture_coords[k + 2][0], model->texture_coords[k + 2][1] };

	vec3 AB, AC;
	glm_vec3_sub(B, A, AB);
	glm_vec3_sub(C, A, AC);

	vec2 deltaUV_AB, deltaUV_AC;
	glm_vec2_sub(UV_B, UV_A, deltaUV_AB);
	glm_vec2_sub(UV_C, UV_A, deltaUV_AC);

	float fract = 1.0f / (deltaUV_AB[0] * deltaUV_AC[1] - deltaUV_AC[0] * deltaUV_AB[1]);

	model->tangents[k][0] = fract * (deltaUV_AC[1] * AB[0] - deltaUV_AB[1] * AC[0]);
	model->tangents[k][1] = fract * (deltaUV_AC[1] * AB[1] - deltaUV_AB[1] * AC[1]);
	model->tangents[k][2] = fract * (deltaUV_AC[1] * AB[2] - deltaUV_AB[1] * AC[2]);

	model->bitangents[k][0] = fract * (-deltaUV_AC[0] * AB[0] + deltaUV_AB[0] * AC[0]);
	model->bitangents[k][1] = fract * (-deltaUV_AC[0] * AB[1] + deltaUV_AB[0] * AC[1]);
	model->bitangents[k][2] = fract * (-deltaUV_AC[0] * AB[2] + deltaUV_AB[0] * AC[2]);

	// TODO: smoothen bitangents and tangents by averaging them for each vector
	model->tangents[k + 1][0] = model->tangents[k + 2][0] = model->tangents[k][0];
	model->tangents[k + 1][1] = model->tangents[k + 2][1] = model->tangents[k][1];
	model->tangents[k + 1][2] = model->tangents[k + 2][2] = model->tangents[k][2];
	model->bitangents[k + 1][0] = model->bitangents[k + 2][0] = model->bitangents[k][0];
	model->bitangents[k + 1][1] = model->bitangents[k + 2][1] = model->bitangents[k][1];
	model->bitangents[k + 1][2] = model->bitangents[k + 2][2] = model->bitangents[k][2];
}

int loadModel(const char* obj_filename, const char *texture_filename, FaceType face_type, bool calculate_tangents)
{
    File file = {0};

    file.vertices = (vec3*) malloc(MAX_OBJ_SIZE * sizeof(vec3));
    file.faces = (Face*) malloc(MAX_OBJ_SIZE * sizeof(Face));
    file.normals = (vec3*) malloc(MAX_OBJ_SIZE * sizeof(vec3));
    file.texture_coords = (vec2*) malloc(MAX_OBJ_SIZE * sizeof(vec2));

    FILE* f = fopen(obj_filename, "r");
    if (!f) abort();

    char buffer[40] = { 0 };
    while ((fscanf(f, " %s", buffer)) != EOF) {
        if (!strcmp(buffer, "v")) {
            float* ptr = file.vertices[++file.num_vertices];
            fscanf(f, " %f %f %f", &ptr[0], &ptr[1], &ptr[2]);
        }
        else if (!strcmp(buffer, "vn")) {
            float* ptr = file.normals[++file.num_normals];
            fscanf(f, " %f %f %f", &ptr[0], &ptr[1], &ptr[2]);
        }
        else if (!strcmp(buffer, "vt")) {
            float* ptr = file.texture_coords[++file.num_texture_coords];
            fscanf(f, " %f %f", &ptr[0], &ptr[1]);
        }
        else if (!strcmp(buffer, "f")) {
            Face face;
            switch (face_type) {
            case VERTEX_ONLY:
                fscanf(f, " %d %d %d",
                       &face.vertices[0], &face.vertices[1], &face.vertices[2]
                );
                break;
            case VERTEX_NORMAL:
                fscanf(f, " %d//%d %d//%d %d//%d",
                       &face.vertices[0], &face.normals[0], &face.vertices[1],
                       &face.normals[1], &face.vertices[2], &face.normals[2]
                );
                break;
            case VERTEX_ALL:
            case VERTEX_ALL_ALPHA:
            case VERTEX_TEXTURE:
                fscanf(f, " %d/%d/%d %d/%d/%d %d/%d/%d",
                       &face.vertices[0], &face.texture_coords[0], &face.normals[0],
                       &face.vertices[1], &face.texture_coords[1], &face.normals[1],
                       &face.vertices[2], &face.texture_coords[2], &face.normals[2]
                );
                break;
            default:
                assert(false);
            }

            file.faces[file.num_faces++] = face;
        }
    }
    fclose(f);

    Model* model = &loaded_models[loaded_models_n];

    if (face_type == VERTEX_ALL || face_type == VERTEX_ALL_ALPHA || face_type == VERTEX_TEXTURE) {
        if (texture_filename) {
            model->has_texture = true;
            model->texture_id = loadTexture(texture_filename);
        } else {
            fprintf(stderr, "Warning: Texture requested, but no filename given!\n");
        }
    }
    model->face_type = face_type;
    model->num_faces = file.num_faces;
    model->vertices = (vec3*) malloc(model->num_faces * 3 * sizeof(vec3));
    model->normals = (vec3*) malloc(model->num_faces * 3 * sizeof(vec3));
    model->texture_coords = (vec2*) malloc(model->num_faces * 3 * sizeof(vec2));

    if (calculate_tangents) {
        model->has_tangents = true;
        model->tangents = (vec3*)malloc(model->num_faces * 3 * sizeof(vec3));
        model->bitangents = (vec3*)malloc(model->num_faces * 3 * sizeof(vec3));
    }

    int k = 0;
    for (int i = 0; i < file.num_faces; i++) {
        for (int j = 0; j < 3; j++) {
            model->vertices[k][j] = file.vertices[file.faces[i].vertices[0]][j];
            model->vertices[k + 1][j] = file.vertices[file.faces[i].vertices[1]][j];
            model->vertices[k + 2][j] = file.vertices[file.faces[i].vertices[2]][j];
        }

        for (int j = 0; j < 3; j++) {
            model->normals[k][j] = file.normals[file.faces[i].normals[0]][j];
            model->normals[k + 1][j] = file.normals[file.faces[i].normals[1]][j];
            model->normals[k + 2][j] = file.normals[file.faces[i].normals[2]][j];
        }

        for (int j = 0; j < 2; j++) {
            model->texture_coords[k][j] = file.texture_coords[file.faces[i].texture_coords[0]][j];
            model->texture_coords[k + 1][j] = file.texture_coords[file.faces[i].texture_coords[1]][j];
            model->texture_coords[k + 2][j] = file.texture_coords[file.faces[i].texture_coords[2]][j];
        }

        // calculate tangents and bitangents if requested (outside this loop, when we have all vertex information loaded)
        if (calculate_tangents) {
            generate_vertex_tangents_and_bitangents(model, k);
		}

        k += 3;
    }

    free(file.vertices);
    free(file.faces);
    free(file.normals);
    free(file.texture_coords);

    return loaded_models_n++;
}

void draw_model_impl(int program, Object obj, bool force_color)
{
    mat4 mat;
    glm_mat4_identity(mat);
    glm_translate(mat, obj.pos);
    double rad = atan2(-obj.dir[1], obj.dir[0]);
    vec3 axis = { 0, 1, 0 };
    glm_rotate(mat, rad, axis);
    glm_scale_uni(mat, obj.scale);
    glUniform1i(glGetUniformLocation(program, "forceColor"), force_color);
    glUniform1i(glGetUniformLocation(program, "shininess"), obj.shininess);
    glUniformMatrix4fv(glGetUniformLocation(program, "model"), 1, GL_FALSE, (const GLfloat*)mat);
    glBindVertexArray(obj.vao);
    glDrawArrays(GL_TRIANGLES, 0, 3 * loaded_models[obj.model_id].num_faces);
    glBindVertexArray(0);
}

void draw_model(int program, Object obj, RenderPass pass)
{
    bool has_texture = loaded_models[obj.model_id].has_texture;
    bool has_normal_map = loaded_models[obj.model_id].has_normal_map;

    if (pass == PASS_FINAL) {
        glUniform1i(glGetUniformLocation(program, "hasTexture"), has_texture);
        glUniform1f(glGetUniformLocation(program, "scaleTexCoords"), obj.scale_tex_coords);
        glUniform1f(glGetUniformLocation(program, "hasNormalMap"), has_normal_map);
        if (has_texture) {
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, loaded_models[obj.model_id].texture_id);
        }
        if (has_normal_map) {
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, loaded_models[obj.model_id].normal_map_id);
        }
    }
        
    draw_model_impl(program, obj, false);
}

void draw_model_force_rgb(int program, Object obj, float r, float g, float b)
{
    vec3 color = { r, g, b };
    glUniform3fv(glGetUniformLocation(program, "forcedColor"), 1, color);
    glUniform1i(glGetUniformLocation(program, "hasTexture"), 0);
    draw_model_impl(program, obj, true);
}

Object create_object(ObjectType type, int model_id, float x, float y, float z, float speed, float scale, float shininess)
{
    Object obj = {};
    glGenVertexArrays(1, &obj.vao);
    obj.type = type;
    obj.pos[0] = x;
    obj.pos[1] = y;
    obj.pos[2] = z;
    obj.model_id = model_id;
    obj.speed = speed;
    obj.scale = scale;
    obj.shininess = shininess;
    obj.scale_tex_coords = 1.0;

	// TODO: perhaps use glGetUniformLocation for vertex indices
    glBindVertexArray(obj.vao);
	    GLuint vbo1, vbo2, vbo3, vbo4, vbo5;
        // TODO: free
        glGenBuffers(1, &vbo1);
        glGenBuffers(1, &vbo2);
        glGenBuffers(1, &vbo3);
        glGenBuffers(1, &vbo4);
        glGenBuffers(1, &vbo5);

        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, vbo1);
        glBufferData(GL_ARRAY_BUFFER, loaded_models[obj.model_id].num_faces * 3 * sizeof(vec3), loaded_models[obj.model_id].vertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, vbo2);
        glBufferData(GL_ARRAY_BUFFER, loaded_models[obj.model_id].num_faces * 3 * sizeof(vec3), loaded_models[obj.model_id].normals, GL_STATIC_DRAW);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

        if (loaded_models[obj.model_id].has_texture) {
            glEnableVertexAttribArray(2);
            glBindBuffer(GL_ARRAY_BUFFER, vbo3);
            glBufferData(GL_ARRAY_BUFFER, loaded_models[obj.model_id].num_faces * 3 * sizeof(vec2), loaded_models[obj.model_id].texture_coords, GL_STATIC_DRAW);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);
        }

        if (loaded_models[obj.model_id].has_normal_map) {
            glEnableVertexAttribArray(3);
            glBindBuffer(GL_ARRAY_BUFFER, vbo4);
            glBufferData(GL_ARRAY_BUFFER, loaded_models[obj.model_id].num_faces * 3 * sizeof(vec3), loaded_models[obj.model_id].tangents, GL_STATIC_DRAW);
            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

            glEnableVertexAttribArray(4);
            glBindBuffer(GL_ARRAY_BUFFER, vbo5);
            glBufferData(GL_ARRAY_BUFFER, loaded_models[obj.model_id].num_faces * 3 * sizeof(vec3), loaded_models[obj.model_id].bitangents, GL_STATIC_DRAW);
            glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);
        }
    glBindVertexArray(0);

    return obj;
}

int create_tiled_plane(int subdivisions, float tile_size, float tex_scale, char *texture_filename) {
    Model *m = &loaded_models[loaded_models_n];

    m->face_type = VERTEX_TEXTURE;
    
    int subdivisions_sq = subdivisions * subdivisions;

    // all tiles + 4 single-quad sides + 1 single-quad bottom
    int num_vertices = subdivisions_sq * 6 + 5 * 6;

    m->num_faces = 2 * subdivisions_sq + 2 * 5;
    m->vertices = (vec3*) malloc(num_vertices * sizeof(vec3));
    m->texture_coords = (vec2*) malloc(num_vertices * sizeof(*m->texture_coords));
    m->normals = (vec3*) malloc(num_vertices * sizeof(*m->normals));
    m->tangents = (vec3*) malloc(num_vertices * sizeof(*m->tangents));
    m->bitangents = (vec3*) malloc(num_vertices * sizeof(*m->bitangents));
    
    int width = subdivisions * 6;
    for (int i = 0; i < subdivisions; i++) {
        for (int j = 0; j < subdivisions; j++) {
            assert(i * width + j * 6 + 5 < num_vertices);

			m->vertices[i * width + j * 6 + 2][0] = j * tile_size;
			m->vertices[i * width + j * 6 + 2][1] = 0.0f;
			m->vertices[i * width + j * 6 + 2][2] = i * tile_size;

			m->vertices[i * width + j * 6 + 1][0] = j * tile_size + tile_size;
			m->vertices[i * width + j * 6 + 1][1] = 0.0f;
			m->vertices[i * width + j * 6 + 1][2] = i * tile_size;

			m->vertices[i * width + j * 6 + 0][0] = j * tile_size;
			m->vertices[i * width + j * 6 + 0][1] = 0.0f;
			m->vertices[i * width + j * 6 + 0][2] = i * tile_size + tile_size;

			m->vertices[i * width + j * 6 + 5][0] = j * tile_size + tile_size;
			m->vertices[i * width + j * 6 + 5][1] = 0.0f;
			m->vertices[i * width + j * 6 + 5][2] = i * tile_size;

			m->vertices[i * width + j * 6 + 4][0] = j * tile_size + tile_size;
			m->vertices[i * width + j * 6 + 4][1] = 0.0f;
			m->vertices[i * width + j * 6 + 4][2] = i * tile_size + tile_size;

			m->vertices[i * width + j * 6 + 3][0] = j * tile_size;
			m->vertices[i * width + j * 6 + 3][1] = 0.0f;
			m->vertices[i * width + j * 6 + 3][2] = i * tile_size + tile_size;

            m->texture_coords[i * width + j * 6 + 2][0] = m->vertices[i * width + j * 6 + 2][0] * tex_scale;
			m->texture_coords[i * width + j * 6 + 2][1] = (1 - m->vertices[i * width + j * 6 + 2][2]) * tex_scale;

            m->texture_coords[i * width + j * 6 + 1][0] = m->vertices[i * width + j * 6 + 1][0] * tex_scale;
			m->texture_coords[i * width + j * 6 + 1][1] = (1 - m->vertices[i * width + j * 6 + 1][2]) * tex_scale;

            m->texture_coords[i * width + j * 6 + 0][0] = m->vertices[i * width + j * 6 + 0][0] * tex_scale;
			m->texture_coords[i * width + j * 6 + 0][1] = (1 - m->vertices[i * width + j * 6 + 0][2]) * tex_scale;

            m->texture_coords[i * width + j * 6 + 5][0] = m->vertices[i * width + j * 6 + 5][0] * tex_scale;
			m->texture_coords[i * width + j * 6 + 5][1] = (1 - m->vertices[i * width + j * 6 + 5][2]) * tex_scale;

            m->texture_coords[i * width + j * 6 + 4][0] = m->vertices[i * width + j * 6 + 4][0] * tex_scale;
			m->texture_coords[i * width + j * 6 + 4][1] = (1 - m->vertices[i * width + j * 6 + 4][2]) * tex_scale;

            m->texture_coords[i * width + j * 6 + 3][0] = m->vertices[i * width + j * 6 + 3][0] * tex_scale;
			m->texture_coords[i * width + j * 6 + 3][1] = (1 - m->vertices[i * width + j * 6 + 3][2]) * tex_scale;

            for (int k = 0; k < 6; k++) {
                m->normals[i * width + j * 6 + k][0] = 0.0f;
                m->normals[i * width + j * 6 + k][1] = 1.0f;
                m->normals[i * width + j * 6 + k][2] = 0.0f;
            }

			generate_vertex_tangents_and_bitangents(m, i * width + j * 6);
			generate_vertex_tangents_and_bitangents(m, i * width + j * 6 + 3);
        }
    }

    // left and right sides
    for (int i = 0; i < 2; i++) {
        m->vertices[subdivisions * width + i * 6 + 0 + (i*2)][0] = i * subdivisions * tile_size;
        m->vertices[subdivisions * width + i * 6 + 0 + (i*2)][1] = 0.0f;
        m->vertices[subdivisions * width + i * 6 + 0 + (i*2)][2] = subdivisions * tile_size;

        m->vertices[subdivisions * width + i * 6 + 1][0] = i * subdivisions * tile_size;
        m->vertices[subdivisions * width + i * 6 + 1][1] = 0.0f;
        m->vertices[subdivisions * width + i * 6 + 1][2] = 0.0f;

        m->vertices[subdivisions * width + i * 6 + 2 - (i*2)][0] = i * subdivisions * tile_size;
        m->vertices[subdivisions * width + i * 6 + 2 - (i*2)][1] = tile_size * -10.0f;
        m->vertices[subdivisions * width + i * 6 + 2 - (i*2)][2] = 0.0f;

        m->vertices[subdivisions * width + i * 6 + 3 + (i*2)][0] = i * subdivisions * tile_size;
        m->vertices[subdivisions * width + i * 6 + 3 + (i*2)][1] = 0.0f;
        m->vertices[subdivisions * width + i * 6 + 3 + (i*2)][2] = subdivisions * tile_size;

        m->vertices[subdivisions * width + i * 6 + 4][0] = i * subdivisions * tile_size;
        m->vertices[subdivisions * width + i * 6 + 4][1] = tile_size * -10.0f;
        m->vertices[subdivisions * width + i * 6 + 4][2] = 0.0f;

        m->vertices[subdivisions * width + i * 6 + 5 - (i*2)][0] = i * subdivisions * tile_size;
        m->vertices[subdivisions * width + i * 6 + 5 - (i*2)][1] = tile_size * -10.0f;
        m->vertices[subdivisions * width + i * 6 + 5 - (i*2)][2] = subdivisions * tile_size;
    }

    // front and back sides
    for (int i = 2; i < 4; i++) {
        m->vertices[subdivisions * width + i * 6 + 0 + ((i-2)*2)][0] = 0.0f;
        m->vertices[subdivisions * width + i * 6 + 0 + ((i-2)*2)][1] = 0.0f;
        m->vertices[subdivisions * width + i * 6 + 0 + ((i-2)*2)][2] = (i-2) * subdivisions * tile_size;

        m->vertices[subdivisions * width + i * 6 + 1][0] = subdivisions * tile_size;
        m->vertices[subdivisions * width + i * 6 + 1][1] = 0.0f;
        m->vertices[subdivisions * width + i * 6 + 1][2] = (i-2) * subdivisions * tile_size;

        m->vertices[subdivisions * width + i * 6 + 2 - ((i-2)*2)][0] = 0.0f;
        m->vertices[subdivisions * width + i * 6 + 2 - ((i-2)*2)][1] = tile_size * -10.0f;
        m->vertices[subdivisions * width + i * 6 + 2 - ((i-2)*2)][2] = (i-2) * subdivisions * tile_size;

        m->vertices[subdivisions * width + i * 6 + 3 + ((i-2)*2)][0] = 0.0f;
        m->vertices[subdivisions * width + i * 6 + 3 + ((i-2)*2)][1] = tile_size * -10.0f;
        m->vertices[subdivisions * width + i * 6 + 3 + ((i-2)*2)][2] = (i-2) * subdivisions * tile_size;

        m->vertices[subdivisions * width + i * 6 + 4][0] = subdivisions * tile_size;
        m->vertices[subdivisions * width + i * 6 + 4][1] = 0.0f;
        m->vertices[subdivisions * width + i * 6 + 4][2] = (i-2) * subdivisions * tile_size;

        m->vertices[subdivisions * width + i * 6 + 5 - ((i-2)*2)][0] = subdivisions * tile_size;
        m->vertices[subdivisions * width + i * 6 + 5 - ((i-2)*2)][1] = tile_size * -10.0f;
        m->vertices[subdivisions * width + i * 6 + 5 - ((i-2)*2)][2] = (i-2) * subdivisions * tile_size;
    }

    // bottom
	m->vertices[subdivisions * width + 4 * 6 + 0][0] = 0.0f;
    m->vertices[subdivisions * width + 4 * 6 + 0][1] = tile_size * -10.0f;
    m->vertices[subdivisions * width + 4 * 6 + 0][2] = 0.0f;

    m->vertices[subdivisions * width + 4 * 6 + 1][0] = subdivisions * tile_size;
	m->vertices[subdivisions * width + 4 * 6 + 1][1] = tile_size * -10.0f;
    m->vertices[subdivisions * width + 4 * 6 + 1][2] = 0.0f;

	m->vertices[subdivisions * width + 4 * 6 + 2][0] = 0.0f;
	m->vertices[subdivisions * width + 4 * 6 + 2][1] = tile_size * -10.0f;
	m->vertices[subdivisions * width + 4 * 6 + 2][2] = subdivisions * tile_size;

	m->vertices[subdivisions * width + 4 * 6 + 3][0] = 0.0f;
	m->vertices[subdivisions * width + 4 * 6 + 3][1] = tile_size * -10.0f;
	m->vertices[subdivisions * width + 4 * 6 + 3][2] = subdivisions * tile_size;

	m->vertices[subdivisions * width + 4 * 6 + 4][0] = subdivisions * tile_size;
	m->vertices[subdivisions * width + 4 * 6 + 4][1] = tile_size * -10.0f;
    m->vertices[subdivisions * width + 4 * 6 + 4][2] = 0.0f;

	m->vertices[subdivisions * width + 4 * 6 + 5][0] = subdivisions * tile_size;
	m->vertices[subdivisions * width + 4 * 6 + 5][1] = tile_size * -10.0f;
	m->vertices[subdivisions * width + 4 * 6 + 5][2] = subdivisions * tile_size;

    if (texture_filename) {
        m->has_texture = true;
		m->texture_id = loadTexture(texture_filename);
    }

    return loaded_models_n++;
}