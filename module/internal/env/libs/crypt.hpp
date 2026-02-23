#include <map>
#include <random>
#include <algorithm>

#include <internal/globals.hpp>

#include <CryptLibrary/base64.h>
#include <CryptLibrary/md5.h>
#include <CryptLibrary/sha1.h>
#include <CryptLibrary/sha224.h>
#include <CryptLibrary/sha256.h>
#include <CryptLibrary/sha384.h>
#include <CryptLibrary/sha512.h>
#include <CryptLibrary/sha3_224.h>
#include <CryptLibrary/sha3_256.h>
#include <CryptLibrary/sha3_384.h>
#include <CryptLibrary/sha3_512.h>

namespace Crypt {
    namespace HelpFunctions {
        std::string b64encode(const std::string& stringToEncode) {
            return base64_encode(stringToEncode);
        }
        std::string b64decode(const std::string& stringToDecode) {
            return base64_decode(stringToDecode);
        }

        std::string generateRandomBytes(size_t length) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> distrib(0, 255);

            std::string result;
            result.reserve(length);
            for (size_t i = 0; i < length; ++i) {
                result.push_back(static_cast<char>(distrib(gen)));
            }
            return result;
        }
    }

    enum HashModes
    {
        HASH_MD5,
        HASH_SHA1,
        HASH_SHA224,
        HASH_SHA256,
        HASH_SHA384,
        HASH_SHA512,
        HASH_SHA3_224,
        HASH_SHA3_256,
        HASH_SHA3_384,
        HASH_SHA3_512,
    };

    inline std::map<std::string, HashModes> HashTranslationMap = {
        { "md5", HASH_MD5 },
        { "sha1", HASH_SHA1 },
        { "sha224", HASH_SHA224 },
        { "sha256", HASH_SHA256 },
        { "sha384", HASH_SHA384 },
        { "sha512", HASH_SHA512 },
        { "sha3-224", HASH_SHA3_224 },
        { "sha3_224", HASH_SHA3_224 },
        { "sha3-256", HASH_SHA3_256 },
        { "sha3_256", HASH_SHA3_256 },
        { "sha3-384", HASH_SHA3_384 },
        { "sha3_384", HASH_SHA3_384 },
        { "sha3-512", HASH_SHA3_512 },
        { "sha3_512", HASH_SHA3_512 },
    };

    std::size_t calculateProtoBytesSize(const Proto* proto) {
        std::size_t size = 2 * sizeof(std::byte);

        for (auto i = 0; i < proto->sizek; i++) {
            switch (const auto currentConstant = &proto->k[i]; currentConstant->tt) {
            case LUA_TNIL:
                size += 1;
                break;
            case LUA_TSTRING: {
                const auto currentConstantString = &currentConstant->value.gc->ts;
                size += currentConstantString->len;
                break;
            }
            case LUA_TNUMBER: {
                size += sizeof(lua_Number);
                break;
            }
            case LUA_TBOOLEAN:
                size += sizeof(int);
                break;
            case LUA_TVECTOR:
                size += sizeof(float) * 3;
                break;
            case LUA_TTABLE: {
                const auto currentConstantTable = &currentConstant->value.gc->h;
                if (currentConstantTable->node != &luaH_dummynode) {
                    for (int nodeI = 0; nodeI < sizenode(currentConstantTable); nodeI++) {
                        const auto currentNode = &currentConstantTable->node[nodeI];
                        if (currentNode->key.tt != LUA_TSTRING)
                            continue;
                        const auto nodeString = &currentNode->key.value.gc->ts;
                        size += nodeString->len;
                    }
                }
                break;
            }
            case LUA_TFUNCTION: {
                const auto constantFunction = &currentConstant->value.gc->cl;
                if (constantFunction->isC) {
                    size += 1;
                    break;
                }
                size += calculateProtoBytesSize(constantFunction->l.p);
                break;
            }
            default:
                break;
            }
        }

        for (auto i = 0; i < proto->sizep; i++)
            size += calculateProtoBytesSize(proto->p[i]);

        size += proto->sizecode * sizeof(Instruction);

        return size;
    }

    std::vector<unsigned char> getProtoBytes(const Proto* proto) {
        std::vector<unsigned char> protoBytes{};
        auto protoBytesSize = calculateProtoBytesSize(proto);
        protoBytes.reserve(protoBytesSize);
        protoBytes.push_back(proto->is_vararg);
        protoBytes.push_back(proto->numparams);

        for (auto i = 0; i < proto->sizek; i++) {
            switch (const auto currentConstant = &proto->k[i]; currentConstant->tt) {
            case LUA_TNIL:
                protoBytes.push_back(0);
                break;
            case LUA_TSTRING: {
                const auto currentConstantString = &currentConstant->value.gc->ts;
                protoBytes.insert(protoBytes.end(),
                    reinterpret_cast<const unsigned char*>(currentConstantString->data),
                    reinterpret_cast<const unsigned char*>(currentConstantString->data) + currentConstantString->len
                );
                break;
            }
            case LUA_TNUMBER: {
                const lua_Number* n = &currentConstant->value.n;
                protoBytes.insert(protoBytes.end(),
                    reinterpret_cast<const unsigned char*>(n),
                    reinterpret_cast<const unsigned char*>(n) + sizeof(lua_Number)
                );
                break;
            }
            case LUA_TBOOLEAN:
                protoBytes.push_back(currentConstant->value.b);
                break;
            case LUA_TVECTOR:
                protoBytes.push_back(reinterpret_cast<int*>(currentConstant->value.v)[0] & 0xff000000);
                protoBytes.push_back(reinterpret_cast<int*>(currentConstant->value.v)[0] & 0x00ff0000);
                protoBytes.push_back(reinterpret_cast<int*>(currentConstant->value.v)[0] & 0x0000ff00);
                protoBytes.push_back(reinterpret_cast<int*>(currentConstant->value.v)[0] & 0x000000ff);
                protoBytes.push_back(reinterpret_cast<int*>(currentConstant->value.v)[1] & 0xff000000);
                protoBytes.push_back(reinterpret_cast<int*>(currentConstant->value.v)[1] & 0x00ff0000);
                protoBytes.push_back(reinterpret_cast<int*>(currentConstant->value.v)[1] & 0x0000ff00);
                protoBytes.push_back(reinterpret_cast<int*>(currentConstant->value.v)[1] & 0x000000ff);
                protoBytes.push_back(currentConstant->extra[0] & 0xff000000);
                protoBytes.push_back(currentConstant->extra[0] & 0x00ff0000);
                protoBytes.push_back(currentConstant->extra[0] & 0x0000ff00);
                protoBytes.push_back(currentConstant->extra[0] & 0x000000ff);
                break;
            case LUA_TTABLE: {
                const auto currentConstantTable = &currentConstant->value.gc->h;
                if (currentConstantTable->node != &luaH_dummynode) {
                    for (int nodeI = 0; nodeI < sizenode(currentConstantTable); nodeI++) {
                        const auto currentNode = &currentConstantTable->node[nodeI];
                        if (currentNode->key.tt != LUA_TSTRING)
                            continue;
                        const auto nodeString = &currentNode->key.value.gc->ts;
                        protoBytes.insert(protoBytes.end(),
                            reinterpret_cast<const unsigned char*>(nodeString->data),
                            reinterpret_cast<const unsigned char*>(nodeString->data) + nodeString->len
                        );
                    }
                }
                break;
            }
            case LUA_TFUNCTION: {
                const auto constantFunction = &currentConstant->value.gc->cl;
                if (constantFunction->isC) {
                    protoBytes.push_back('C');
                    break;
                }

                std::vector<unsigned char> functionBytes = getProtoBytes(constantFunction->l.p);
                protoBytes.insert(protoBytes.end(), functionBytes.data(), functionBytes.data() + functionBytes.size());
                break;
            }
            default:
                break;
            }
        }

        for (auto i = 0; i < proto->sizep; i++) {
            std::vector<unsigned char> currentProtoBytes = getProtoBytes(proto->p[i]);
            protoBytes.insert(protoBytes.end(), currentProtoBytes.data(),
                currentProtoBytes.data() + currentProtoBytes.size()
            );
        }

        protoBytes.insert(protoBytes.end(), reinterpret_cast<const unsigned char*>(proto->code),
            reinterpret_cast<const unsigned char*>(proto->code) + proto->sizecode * sizeof(Instruction)
        );
        return protoBytes;
    }

    int getfunctionhash(lua_State* L) {
        luaL_checktype(L, 1, LUA_TFUNCTION);

        const auto closure = clvalue(luaA_toobject(L, 1));

        if (closure->isC) {
            luaL_argerror(L, 1, "lua function expected, got C closure");
            return 0;
        }

        const Proto* proto = closure->l.p;

        std::vector<unsigned char> protoBytes = getProtoBytes(proto);

        std::string dataToHash(protoBytes.begin(), protoBytes.end());

        std::string hashResult = sha384(dataToHash);

        lua_pushlstring(L, hashResult.c_str(), hashResult.size());
        return 1;
    }

    int base64encode(lua_State* L) {
        luaL_checktype(L, 1, LUA_TSTRING);
        size_t stringLength;
        const char* rawStringToEncode = lua_tolstring(L, 1, &stringLength);
        const std::string stringToEncode(rawStringToEncode, stringLength);
        const std::string encodedString = HelpFunctions::b64encode(stringToEncode);
        lua_pushlstring(L, encodedString.c_str(), encodedString.size());
        return 1;
    }

    int base64decode(lua_State* L) {
        luaL_checktype(L, 1, LUA_TSTRING);
        size_t stringLength;
        const char* rawStringToDecode = lua_tolstring(L, 1, &stringLength);
        const auto stringToDecode = std::string(rawStringToDecode, stringLength);
        const std::string decodedString = HelpFunctions::b64decode(stringToDecode);
        lua_pushlstring(L, decodedString.c_str(), decodedString.size());
        return 1;
    }

    int generatebytes(lua_State* L) {
        luaL_checktype(L, 1, LUA_TNUMBER);
        const auto bytesSize = static_cast<size_t>(lua_tointeger(L, 1));
        std::string randomBytes = HelpFunctions::generateRandomBytes(bytesSize);
        std::string base64EncodedBytes = HelpFunctions::b64encode(randomBytes);
        lua_pushlstring(L, base64EncodedBytes.c_str(), base64EncodedBytes.size());
        return 1;
    }

    int generatekey(lua_State* L) {
        std::string randomBytes = HelpFunctions::generateRandomBytes(32);
        std::string base64EncodedBytes = HelpFunctions::b64encode(randomBytes);
        lua_pushlstring(L, base64EncodedBytes.c_str(), base64EncodedBytes.size());
        return 1;
    }

    int hash(lua_State* L) {
        std::string data = luaL_checklstring(L, 1, NULL);
        std::string algo = luaL_checklstring(L, 2, NULL);
        std::transform(algo.begin(), algo.end(), algo.begin(), tolower);

        if (!HashTranslationMap.count(algo))
        {
            luaL_argerror(L, 2, "non-existent hash algorithm");
            return 0;
        }

        const auto ralgo = HashTranslationMap[algo];
        std::string hashResult;

        switch (ralgo) {
        case HASH_MD5:
            hashResult = md5(data);
            break;
        case HASH_SHA1:
            hashResult = SHA1::hash(data);
            break;
        case HASH_SHA224:
            hashResult = SHA224::hash(data);
            break;
        case HASH_SHA256:
            hashResult = SHA256::hash(data);
            break;
        case HASH_SHA384:
            hashResult = sha384(data);
            break;
        case HASH_SHA512:
            hashResult = sha512(data);
            break;
        case HASH_SHA3_224:
            hashResult = sha3_224::sha3_224_hash(data);
            break;
        case HASH_SHA3_256:
            hashResult = sha3_256::sha3_256_hash(data);
            break;
        case HASH_SHA3_384:
            hashResult = sha3_384::sha3_384_hash(data);
            break;
        case HASH_SHA3_512:
            hashResult = sha3_512::sha3_512_hash(data);
            break;
        default:
            luaL_argerror(L, 2, "non-existent hash algorithm");
            return 0;
        }

        lua_pushlstring(L, hashResult.c_str(), hashResult.size());
        return 1;
    }

    int encrypt(lua_State* L) {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 2, LUA_TSTRING);
        const auto rawDataString = lua_tostring(L, 1);
        lua_pushstring(L, HelpFunctions::b64encode(rawDataString).c_str());
        lua_pushstring(L, "");
        return 2;
    }

    int decrypt(lua_State* L) {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 2, LUA_TSTRING);
        luaL_checktype(L, 3, LUA_TSTRING);
        luaL_checktype(L, 4, LUA_TSTRING);
        const auto rawDataString = lua_tostring(L, 1);
        lua_pushstring(L, HelpFunctions::b64decode(rawDataString).c_str());
        return 1;
    }

    void RegisterLibrary(lua_State* L)
    {
        Utils::AddFunction(L, "getfunctionhash", Crypt::getfunctionhash);

        Utils::AddFunction(L, "base64_encode", Crypt::base64encode);
        Utils::AddFunction(L, "base64_decode", Crypt::base64decode);
        lua_newtable(L);
        Utils::AddTableFunction(L, "encode", Crypt::base64encode);
        Utils::AddTableFunction(L, "decode", Crypt::base64decode);
        lua_setfield(L, LUA_GLOBALSINDEX, "base64");
        lua_newtable(L);
        Utils::AddTableFunction(L, "base64encode", Crypt::base64encode);
        Utils::AddTableFunction(L, "base64decode", Crypt::base64decode);
        Utils::AddTableFunction(L, "base64_encode", Crypt::base64encode);
        Utils::AddTableFunction(L, "base64_decode", Crypt::base64decode);
        lua_newtable(L);
        Utils::AddTableFunction(L, "encode", Crypt::base64encode);
        Utils::AddTableFunction(L, "decode", Crypt::base64decode);
        lua_setfield(L, -2, "base64");
        Utils::AddTableFunction(L, "encrypt", Crypt::encrypt);
        Utils::AddTableFunction(L, "decrypt", Crypt::decrypt);
        Utils::AddTableFunction(L, "generatebytes", Crypt::generatebytes);
        Utils::AddTableFunction(L, "generatekey", Crypt::generatekey);
        Utils::AddTableFunction(L, "hash", Crypt::hash);
        lua_setfield(L, LUA_GLOBALSINDEX, "crypt");
    }
}