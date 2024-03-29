#include "fbx_loader.h"
#define NAME_OF(VariableName) # VariableName

ModelAsset* FbxLoader::Load(const char* path)
{
	model_data_ = nullptr;
	model_data_ = Object::Create<ModelAsset>();

	// fbx sdk初期化処理
	FbxManager* manager = FbxManager::Create();
	FbxScene* scene = FbxScene::Create(manager, "");

	FbxImporter* importer = FbxImporter::Create(manager, "");
	importer->Initialize(path, -1, manager->GetIOSettings());
	bool isImporting = importer->Import(scene);

	if (!isImporting)
	{
		std::cerr << "faild import path:" << path << std::endl;
		return nullptr;
	}

	importer->Destroy();

	// 三角形に変換
	FbxGeometryConverter geometry_converter(manager);
	geometry_converter.Triangulate(scene, true);

	// モデルの読み込み
	FbxMesh* mesh = scene->GetSrcObject<FbxMesh>();

	if (mesh == nullptr) {
		std::cerr << "failed load mesh:" << path << std::endl;
		return nullptr;
	}

	LoadVertex(mesh);
	LoadVertexIndex(mesh);
	LoadNormal(mesh);
	LoadUv(mesh);
	LoadVertexColor(mesh);

	FbxSurfaceMaterial* material = scene->GetMaterial(0);

	if (material) {
		LoadMaterial(material);
	}

	manager->Destroy();

	return model_data_;
}

void FbxLoader::LoadVertex(FbxMesh* mesh)
{
	int controlPointsCount = mesh->GetControlPointsCount();
	model_data_->vertices_count_ = controlPointsCount;
	model_data_->vertices_ = new Vertex[controlPointsCount];

	for (int i = 0; i < controlPointsCount; i++)
	{
		FbxVector4 point = mesh->GetControlPointAt(i);
		Vertex vertex;
		vertex.position[0] = point[0];
		vertex.position[1] = point[1];
		vertex.position[2] = point[2];

		vertex.color[0] = 1;
		vertex.color[1] = 1;
		vertex.color[2] = 1;
		vertex.color[3] = 1;

		model_data_->vertices_[i] = vertex;
	}
}

void FbxLoader::LoadVertexIndex(FbxMesh* mesh)
{
	int size = mesh->GetPolygonVertexCount();
	std::cout << size << std::endl;
	model_data_->indices_count_ = size;
	model_data_->indices_ = new GLuint[size];

	for (int i = 0; i < size; i++) {
		model_data_->indices_[i] = mesh->GetPolygonVertices()[i];
	}
}

void FbxLoader::LoadNormal(FbxMesh* mesh)
{
	int normal_count = mesh->GetElementNormalCount();
	for (int i = 0; i < normal_count; i++)
	{
		FbxGeometryElementNormal* normal = mesh->GetElementNormal(i);
		FbxGeometryElement::EMappingMode  mapping_mode = normal->GetMappingMode();
		FbxGeometryElement::EReferenceMode reference_mode = normal->GetReferenceMode();

		switch (mapping_mode)
		{
		case fbxsdk::FbxLayerElement::eNone:
			break;
		case fbxsdk::FbxLayerElement::eByControlPoint:

			switch (reference_mode)
			{
			case fbxsdk::FbxLayerElement::eDirect:
				LoadNormalByControllPointAndDirect(normal);
				break;
			case fbxsdk::FbxLayerElement::eIndex:
				break;
			case fbxsdk::FbxLayerElement::eIndexToDirect:
				break;
			}

			break;
		case fbxsdk::FbxLayerElement::eByPolygonVertex:
			break;
		case fbxsdk::FbxLayerElement::eByPolygon:
			break;
		case fbxsdk::FbxLayerElement::eByEdge:
			break;
		case fbxsdk::FbxLayerElement::eAllSame:
			break;
		}

	}
}

void FbxLoader::LoadNormalByControllPointAndDirect(FbxGeometryElementNormal* normal)
{
	for (int i = 0; i < normal->GetDirectArray().GetCount(); i++)
	{
		model_data_->vertices_[i].normal[0] = static_cast<GLfloat>(normal->GetDirectArray().GetAt(i)[0]);
		model_data_->vertices_[i].normal[1] = static_cast<GLfloat>(normal->GetDirectArray().GetAt(i)[1]);
		model_data_->vertices_[i].normal[2] = static_cast<GLfloat>(normal->GetDirectArray().GetAt(i)[2]);
	}
}

void FbxLoader::LoadUv(FbxMesh* mesh)
{
	FbxStringList uv_set_names;
	mesh->GetUVSetNames(uv_set_names);

	for (int i = 0; i < uv_set_names.GetCount(); i++)
	{
		std::string uv_set_name = uv_set_names.GetStringAt(i);
		FbxGeometryElementUV* uv_element = mesh->GetElementUV(uv_set_name.c_str());

		if (!uv_element)
		{
			continue;
		}

		for (int j = 0; j < (sizeof(model_data_->indices_) / sizeof(GLuint)); j++) {
			FbxVector2 point = uv_element->GetDirectArray().GetAt(model_data_->indices_[i]);
		}

		FbxGeometryElement::EMappingMode mapping_mode = uv_element->GetMappingMode();
		FbxGeometryElement::EReferenceMode reference_mode = uv_element->GetReferenceMode();

		if (mapping_mode != FbxGeometryElement::eByPolygonVertex &&
			mapping_mode != FbxGeometryElement::eByControlPoint)
		{
			return;
		}

		bool is_use_index = reference_mode != FbxGeometryElement::eDirect;
		int index_count = (is_use_index) ? uv_element->GetIndexArray().GetCount() : 0;

		// 頂点と同じ
		if (mapping_mode == FbxGeometryElement::eByControlPoint)
		{
			int index = 0;
			std::vector<int> indices;
			for (int j = 0; j < mesh->GetPolygonCount(); j++)
			{
				for (int k = 0; k < 3; k++)
				{
					int polygon_vertex_index = mesh->GetPolygonVertex(j, k);
					int uv_index = is_use_index ? uv_element->GetIndexArray().GetAt(polygon_vertex_index) : polygon_vertex_index;
					FbxVector2 uv_point = uv_element->GetDirectArray().GetAt(uv_index);

					auto itr = std::find(indices.begin(), indices.end(), polygon_vertex_index);

					if (itr == indices.end())
					{
						indices.push_back(polygon_vertex_index);
						model_data_->vertices_[index].uv[0] = uv_point[0];
						model_data_->vertices_[index].uv[1] = uv_point[1];
						index++;
					}
				}
			}
		}
		else if (mapping_mode == FbxGeometryElement::eByPolygonVertex)
		{
			int polygon_counter = 0;
			for (int j = 0; j < mesh->GetPolygonCount(); j++)
			{
				int polygon_size = mesh->GetPolygonSize(j);
				for (int k = 0; k < polygon_size; k++)
				{
					if (polygon_counter < index_count)
					{
						int uv_vertex_index = is_use_index ? uv_element->GetIndexArray().GetAt(polygon_counter) : polygon_counter;
						FbxVector2 uv_point = uv_element->GetDirectArray().GetAt(uv_vertex_index);
						Uv uv;
						uv.point[0] = uv_point[0];
						uv.point[1] = uv_point[1];
						model_data_->uv_points_.push_back(uv);
						polygon_counter++;
					}
				}
			}
		}
	}
}

void FbxLoader::AddTexture(FbxProperty prop)
{
	int layered_texture_count = prop.GetSrcObjectCount<FbxLayeredTexture>();

	if (0 < layered_texture_count) {
		int file_texture_count = prop.GetSrcObjectCount<FbxFileTexture>();

		if (0 < file_texture_count) {
			for (int i = 0; file_texture_count > i; i++) {
				FbxFileTexture* texture = prop.GetSrcObject<FbxFileTexture>(i);
				if (texture) {
					std::string texture_path = texture->GetRelativeFileName();
				}
			}
		}

		for (int i = 0; layered_texture_count > i; i++) {
			FbxLayeredTexture* layered_texture = prop.GetSrcObject<FbxLayeredTexture>(i);
			int textureCount = layered_texture->GetSrcObjectCount<FbxFileTexture>();

			for (int j = 0; textureCount > j; j++) {
				FbxFileTexture* texture = prop.GetSrcObject<FbxFileTexture>(j);

				if (texture) {
					std::string texture_name = texture->GetRelativeFileName();
					std::string uv_set_name = texture->UVSet.Get().Buffer();

					model_data_->texture_names_.push_back(texture_name);
					model_data_->uv_set_name_ = uv_set_name;
				}
			}
		}
	}
	else {
		int file_texture_count = prop.GetSrcObjectCount<FbxFileTexture>();

		if (0 < file_texture_count) {
			for (int i = 0; file_texture_count > i; i++) {
				FbxFileTexture* texture = prop.GetSrcObject<FbxFileTexture>(i);
				if (texture) {
					std::string texture_name = texture->GetRelativeFileName();
					std::string uv_set_name = texture->UVSet.Get().Buffer();
					model_data_->uv_set_name_ = uv_set_name;
					model_data_->texture_names_.push_back(texture_name);
				}
			}
		}
	}
}

void FbxLoader::LoadVertexColor(FbxMesh* mesh)
{
	int vertex_color_count = mesh->GetElementVertexColorCount();

	for (int i = 0; i < vertex_color_count; i++)
	{
		FbxGeometryElementVertexColor* vertex_color = mesh->GetElementVertexColor(i);
		FbxGeometryElement::EMappingMode mapping_mode = vertex_color->GetMappingMode();
		FbxGeometryElement::EReferenceMode reference_mode = vertex_color->GetReferenceMode();

		switch (mapping_mode)
		{
		case fbxsdk::FbxLayerElement::eNone:
			break;
		case fbxsdk::FbxLayerElement::eByControlPoint:
			break;
		case fbxsdk::FbxLayerElement::eByPolygonVertex:
			break;
		case fbxsdk::FbxLayerElement::eByPolygon:
			if (reference_mode == FbxGeometryElement::EReferenceMode::eDirect)
			{
				LoadVertexColorByPolygonVertexAndIndexToDirect(vertex_color);
			}
			break;
		case fbxsdk::FbxLayerElement::eByEdge:
			break;
		case fbxsdk::FbxLayerElement::eAllSame:
			break;
		default:
			break;
		}
	}
}

void FbxLoader::LoadVertexColorByPolygonVertexAndIndexToDirect(FbxGeometryElementVertexColor* vertex_color)
{
	FbxLayerElementArrayTemplate<int>* index = &vertex_color->GetIndexArray();
	int index_count = index->GetCount();
	float r, g, b, a;

	for (int i = 0; i < index_count; i++)
	{
		r = static_cast<GLfloat>(vertex_color->GetDirectArray().GetAt(i)[0]);
		g = static_cast<GLfloat>(vertex_color->GetDirectArray().GetAt(i)[1]);
		b = static_cast<GLfloat>(vertex_color->GetDirectArray().GetAt(i)[2]);
		a = static_cast<GLfloat>(vertex_color->GetDirectArray().GetAt(i)[3]);
	}
}

void FbxLoader::LoadMaterial(FbxSurfaceMaterial* mat)
{
	FbxProperty diffuse = mat->FindProperty(mat->sDiffuse);
	if (diffuse.IsValid()) {
		std::cout << mat->sDiffuse;
		AddTexture(diffuse);
	}

	FbxProperty ambient = mat->FindProperty(mat->sAmbient);
	if (ambient.IsValid()) {
		std::cout << mat->sAmbient;
		AddTexture(ambient);
	}

	FbxProperty emissive = mat->FindProperty(mat->sEmissive);
	if (emissive.IsValid()) {
		std::cout << mat->sEmissive;
		AddTexture(emissive);
	}

	FbxProperty specular = mat->FindProperty(mat->sSpecular);
	if (specular.IsValid()) {
		std::cout << mat->sSpecular;
		AddTexture(specular);
	}

	FbxProperty transparent_color = mat->FindProperty(mat->sTransparentColor);
	if (transparent_color.IsValid()) {
		// std::cout << mat->sTransparentColor;
		// AddTexture(transparent_color);
	}
}
