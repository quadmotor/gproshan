#include "che_obj.h"

#include <fstream>
#include <vector>
#include <cstring>
#include <cassert>


che_obj::che_obj(const string & file)
{
	init(file);
}

che_obj::che_obj(const che_obj & mesh): che(mesh)
{
}

void che_obj::read_file(const string & file)
{
	ifstream is(file);

	assert(is.good());
	
	real_t x, y, z;
	index_t face[8], i;

	vector<vertex> vertices;
	vector<index_t> faces;

	char line[128];
	string key;

	while(is.getline(line, sizeof(line)))
	{
		stringstream ss(line);

		ss >> key;
		if(key == "") continue;

		if(key == "v")
		{
			ss >> x >> y >> z;
			vertices.push_back({x, y, z});
		}

		if(key == "f")
		{
			for(i = 0; ss >> face[i]; i++)
				ss.ignore(256, ' ');
			
			if(i == che::P) // che::P = 3, triangular mesh
			{
				faces.push_back(face[0] - 1);	
				faces.push_back(face[1] - 1);	
				faces.push_back(face[2] - 1);	
			}
			else if(i == 4) // quadrangular mesh, split two triangles
			{
				faces.push_back(face[0] - 1);	
				faces.push_back(face[1] - 1);	
				faces.push_back(face[3] - 1);

				faces.push_back(face[1] - 1);	
				faces.push_back(face[2] - 1);	
				faces.push_back(face[3] - 1);
			}
		}
	}

	is.close();
	
	init(vertices.data(), vertices.size(), faces.data(), faces.size() / che::P);
}

void che_obj::write_file(const che * mesh, const string & file)
{
	ofstream os(file + ".obj");

	os << "####\n#\n";
	os << "# OBJ generated by gproshan 2019" << endl;
	os << "# vertices: " << mesh->n_vertices() << endl;
	os << "# faces: " << mesh->n_faces() << endl;
	os << "#\n####\n";

	for(size_t v = 0; v < mesh->n_vertices(); v++)
		os << "v " << mesh->gt(v) << endl;

	for(index_t he = 0; he < mesh->n_half_edges(); )
	{
		os << "f";
		for(index_t i = 0; i < che::P; i++)
			os << " " << mesh->vt(he++) + 1;
		os << endl;
	}

	os.close();
}

