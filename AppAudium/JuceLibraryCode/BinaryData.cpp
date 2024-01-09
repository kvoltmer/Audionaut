/* ==================================== JUCER_BINARY_RESOURCE ====================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#include <cstring>

namespace BinaryData
{

//================== zoomin.svg ==================
static const unsigned char temp_binary_data_0[] =
"<svg width=\"256\" height=\"256\" xmlns=\"http://www.w3.org/2000/svg\">\n"
"    <g fill-rule=\"evenodd\">\n"
"        <path d=\"M120.46 158.29c-30.166 8.65-61.631-8.792-70.281-38.957-8.65-30.165 8.792-61.63 38.957-70.28 30.165-8.65 61.63 8.792 70.28 38.957 4.417 15.403-1.937 38.002-9.347 50.872-.614 1.067 59.212 53.064 59.212 53.064l-17.427 17.63-53.514-56.7"
"2s-10.233 3.241-17.88 5.434zM104 144c22.091 0 40-17.909 40-40s-17.909-40-40-40-40 17.909-40 40 17.909 40 40 40z\"/>\n"
"        <path d=\"M111.912 80.047h-15.95v16.037H80v15.992h15.962V128h15.95v-15.924H128V96.084h-16.088z\"/>\n"
"    </g>\n"
"</svg>\n";

const char* zoomin_svg = (const char*) temp_binary_data_0;


const char* getNamedResource (const char* resourceNameUTF8, int& numBytes);
const char* getNamedResource (const char* resourceNameUTF8, int& numBytes)
{
    unsigned int hash = 0;

    if (resourceNameUTF8 != nullptr)
        while (*resourceNameUTF8 != 0)
            hash = 31 * hash + (unsigned int) *resourceNameUTF8++;

    switch (hash)
    {
        case 0xfebd99dd:  numBytes = 579; return zoomin_svg;
        default: break;
    }

    numBytes = 0;
    return nullptr;
}

const char* namedResourceList[] =
{
    "zoomin_svg"
};

const char* originalFilenames[] =
{
    "zoomin.svg"
};

const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8)
{
    for (unsigned int i = 0; i < (sizeof (namedResourceList) / sizeof (namedResourceList[0])); ++i)
        if (strcmp (namedResourceList[i], resourceNameUTF8) == 0)
            return originalFilenames[i];

    return nullptr;
}

}
