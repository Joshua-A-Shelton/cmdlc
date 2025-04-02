#include <filesystem>
#include <fstream>
#include <iostream>
#include <boost/program_options.hpp>

#include <assimp/Importer.hpp>

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <lz4.h>
#include <boost/endian/conversion.hpp>

namespace po = boost::program_options;

uint16_t POSITION_3D = 0b0000000000000001;
uint16_t POSITION_2D = 0b0000000000000010;
uint16_t NORMAL = 0b0000000000000100;
uint16_t UV = 0b0000000000001000;
uint16_t COLOR = 0b0000000000010000;
uint16_t BONE_WEIGHT = 0b0000000000100000;



void writeIndices(std::stringstream& out, aiMesh* mesh)
{
    if constexpr(std::endian::native == std::endian::big)
    {
        boost::endian::native_to_little_inplace(POSITION_3D);
        boost::endian::native_to_little_inplace(POSITION_2D);
        boost::endian::native_to_little_inplace(NORMAL);
        boost::endian::native_to_little_inplace(UV);
        boost::endian::native_to_little_inplace(COLOR);
        boost::endian::native_to_little_inplace(BONE_WEIGHT);
    }
    uint8_t int32 = 0;
    if (mesh->mNumVertices > UINT16_MAX)
    {
        int32 = 1;
        out.write((char*)&int32, sizeof(uint8_t));
        std::vector<uint32_t> indexes;
        for (auto j=0; j<mesh->mNumFaces; j++)
        {
            auto& face = mesh->mFaces[j];
            if (face.mNumIndices != 3)
            {
                throw std::runtime_error("mesh isn't triangulated");
            }
            indexes.push_back(face.mIndices[0]);
            indexes.push_back(face.mIndices[1]);
            indexes.push_back(face.mIndices[2]);
        }
        int32_t uncompressedSize = indexes.size()*sizeof(int32_t);
        int32_t compressedSize = LZ4_compressBound(uncompressedSize);
        std::vector<char> compressedData(compressedSize);
        compressedSize = LZ4_compress_fast(reinterpret_cast<const char*>(indexes.data()),compressedData.data(),uncompressedSize,compressedSize,1);
        out.write((char*)&compressedSize, sizeof(int32_t));
        out.write((char*)&uncompressedSize, sizeof(int32_t));
        out.write(compressedData.data(), compressedSize);
    }
    else
    {
        out.write((char*)&int32, sizeof(uint8_t));
        std::vector<uint16_t> indexes;
        for (auto j=0; j<mesh->mNumFaces; j++)
        {
            auto& face = mesh->mFaces[j];
            if (face.mNumIndices != 3)
            {
                throw std::runtime_error("mesh isn't triangulated");
            }
            indexes.push_back(face.mIndices[0]);
            indexes.push_back(face.mIndices[1]);
            indexes.push_back(face.mIndices[2]);
        }
        int32_t uncompressedSize = indexes.size()*sizeof(int16_t);
        int32_t compressedSize = LZ4_compressBound(uncompressedSize);
        std::vector<char> compressedData(compressedSize);
        compressedSize = LZ4_compress_fast(reinterpret_cast<const char*>(indexes.data()),compressedData.data(),uncompressedSize,compressedSize,1);
        out.write((char*)&compressedSize, sizeof(int32_t));
        out.write((char*)&uncompressedSize, sizeof(int32_t));
        out.write(compressedData.data(), compressedSize);
    }
}

int main(int argc, char**argv)
{
    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message")
    ("input,i", po::value<std::string>(), "input file")
    ("output,o", po::value<std::string>(), "output file")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc,argv,desc),vm);

    if (vm.count("help"))
    {
        std::cout << desc << std::endl;
        return 0;
    }

    std::filesystem::path inputFile = vm["input"].as<std::string>();
    std::filesystem::path outputFile = vm["output"].as<std::string>();


    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(inputFile.string(),aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_FlipUVs);
    if (scene && scene->HasMeshes())
    {
        std::ofstream outFile(outputFile, std::ios::out | std::ios::binary | std::ios::trunc);
        outFile << "cmodel\n";
        uint32_t meshCount = scene->mNumMeshes;
        outFile.write((char*)&meshCount, sizeof(uint32_t));

        for (auto i = 0; i < scene->mNumMeshes; i++)
        {
            aiMesh* mesh = scene->mMeshes[i];
            uint8_t vertexAttributeCount = 0;
            if (mesh->HasPositions())
            {
                vertexAttributeCount++;
            }
            if (mesh->HasNormals())
            {
                vertexAttributeCount++;
            }
            if (mesh->HasTextureCoords(0))
            {
                vertexAttributeCount++;
            }
            if (mesh->HasVertexColors(0))
            {
                vertexAttributeCount++;
            }
            if (mesh->HasBones())
            {
                //vertexAttributeCount++;
            }

            std::stringstream outStream( std::ios::out | std::ios::binary);

            //Write out index buffer
            writeIndices(outStream, mesh);

            //Write out attribute buffers

            //atrribute count
            outStream.write((char*)&vertexAttributeCount, sizeof(uint8_t));
            if (mesh->HasPositions())
            {
                outStream.write((char*)&POSITION_3D, sizeof(uint16_t));
                int32_t uncompressedSize = mesh->mNumVertices*3*sizeof(float);
                int32_t compressedSize = LZ4_compressBound(uncompressedSize);
                std::vector<char> compressedData(compressedSize);
                compressedSize = LZ4_compress_fast((char*)mesh->mVertices,compressedData.data(),uncompressedSize,compressedSize,1);
                outStream.write((char*)&compressedSize, sizeof(int32_t));
                outStream.write((char*)&uncompressedSize, sizeof(int32_t));
                outStream.write(compressedData.data(), compressedSize);
            }
            if (mesh->HasNormals())
            {
                outStream.write((char*)&NORMAL, sizeof(uint16_t));
                int32_t uncompressedSize = mesh->mNumVertices*3*sizeof(float);
                int32_t compressedSize = LZ4_compressBound(uncompressedSize);
                std::vector<char> compressedData(compressedSize);
                compressedSize = LZ4_compress_fast((char*)mesh->mNormals,compressedData.data(),uncompressedSize,compressedSize,1);
                outStream.write((char*)&compressedSize, sizeof(int32_t));
                outStream.write((char*)&uncompressedSize, sizeof(int32_t));
                outStream.write(compressedData.data(), compressedSize);
            }
            if (mesh->HasTextureCoords(0))
            {
                struct vec2{float x,y;};
                std::vector<vec2> texCoords(mesh->mNumVertices);
                for (auto i = 0; i < mesh->mNumVertices; i++)
                {
                    auto uvData = mesh->mTextureCoords[0][i];
                    texCoords[i] = {.x = uvData.x,.y = uvData.y};
                }
                outStream.write((char*)&UV, sizeof(uint16_t));
                int32_t uncompressedSize = mesh->mNumVertices*2*sizeof(float);
                int32_t compressedSize = LZ4_compressBound(uncompressedSize);
                std::vector<char> compressedData(compressedSize);
                compressedSize = LZ4_compress_fast((char*)&texCoords[0],compressedData.data(),uncompressedSize,compressedSize,1);
                outStream.write((char*)&compressedSize, sizeof(int32_t));
                outStream.write((char*)&uncompressedSize, sizeof(int32_t));
                outStream.write(compressedData.data(), compressedSize);
            }
            if (mesh->HasVertexColors(0))
            {
                outStream.write((char*)&COLOR, sizeof(uint16_t));
                int32_t uncompressedSize = mesh->mNumVertices*4*sizeof(unsigned char);
                int32_t compressedSize = LZ4_compressBound(uncompressedSize);
                std::vector<char> compressedData(compressedSize);
                std::vector<unsigned char> uncompressedData(uncompressedSize);
                for (auto vertNumber = 0; vertNumber < mesh->mNumVertices; vertNumber++)
                {
                    auto color = mesh->mColors[0][vertNumber];
                    unsigned char red = color.r * 255;
                    unsigned char green = color.g * 255;
                    unsigned char blue = color.b * 255;
                    unsigned char alpha = color.a * 255;
                    uncompressedData[vertNumber*4] = red;
                    uncompressedData[vertNumber*4+1] = green;
                    uncompressedData[vertNumber*4+2] = blue;
                    uncompressedData[vertNumber*4+3] = alpha;

                }

                compressedSize = LZ4_compress_fast((char*)uncompressedData.data(),compressedData.data(),uncompressedSize,compressedSize,1);
                outStream.write((char*)&compressedSize, sizeof(int32_t));
                outStream.write((char*)&uncompressedSize, sizeof(int32_t));
                outStream.write(compressedData.data(), compressedSize);
            }
            if (mesh->HasBones())
            {
                std::cout << "Bones not currently supported\n";
            }
            auto string = outStream.str();
            uint64_t meshLength = outStream.tellp();
            outFile.write((char*)&meshLength, sizeof(uint64_t));
            for (auto i=0; i< string.length(); i++)
            {
                outFile << ((unsigned char)string[i]);
            }
        }
    }
    else
    {
        std::cout << importer.GetErrorString() << std::endl;
    }
    return 0;
}
