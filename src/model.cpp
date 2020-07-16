void destroyModel(int model) {
    free(loaded_models[model].vertices);
    free(loaded_models[model].normals);
    free(loaded_models[model].texture_coords);

    if (loaded_models[model].face_type == VERTEX_ALL || loaded_models[model].face_type == VERTEX_ALL_ALPHA) {
        glDeleteTextures(1, &loaded_models[model].texture_id);
    }
}

int loadModel(const char* obj_filename, const char* texture_filename, FaceType face_type, int texture_size) {
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

    if (face_type == VERTEX_ALL || face_type == VERTEX_ALL_ALPHA) {
        // TODO
        //model->texture_id = loadTexture(texture_filename, face_type, texture_size);
    }
    model->face_type = face_type;
    model->num_faces = file.num_faces;
    model->vertices = (vec3*) malloc(model->num_faces * 3 * sizeof(vec3));
    model->normals = (vec3*) malloc(model->num_faces * 3 * sizeof(vec3));
    model->texture_coords = (vec2*) malloc(model->num_faces * 3 * sizeof(vec2));

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

        k += 3;
    }

    free(file.vertices);
    free(file.faces);
    free(file.normals);
    free(file.texture_coords);

    return loaded_models_n++;
}
