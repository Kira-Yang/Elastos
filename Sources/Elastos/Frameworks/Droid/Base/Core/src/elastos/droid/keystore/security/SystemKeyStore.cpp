#include "Elastos.CoreLibrary.IO.h"
#include "Elastos.CoreLibrary.Libcore.h"
#include "Elastos.CoreLibrary.Security.h"
#include "Elastos.CoreLibrary.Extensions.h"
#include "elastos/droid/keystore/security/SystemKeyStore.h"
#include "elastos/core/StringBuilder.h"
#include "elastos/core/StringUtils.h"
#include "elastos/droid/os/FileUtils.h"
#include "elastos/droid/os/Environment.h"
#include <elastos/utility/logging/Logger.h>

using Elastos::Droid::Os::FileUtils;
using Elastos::Droid::Os::Environment;
//import android.os.Environment;
//import android.os.FileUtils;
//
//import java.io.File;
//import java.io.FileOutputStream;
using Elastos::IO::CFile;
using Elastos::IO::IOutputStream;
using Elastos::IO::IFileOutputStream;
using Elastos::IO::CFileOutputStream;
using Elastos::IO::ICloseable;
using Elastos::IO::IFlushable;
//import java.io.IOException;
//import java.security.NoSuchAlgorithmException;
//import java.security.SecureRandom;
using Elastos::Security::IKey;
using Elastos::Security::ISecureRandom;
using Elastos::Security::ISecureRandomHelper;
using Elastos::Security::CSecureRandomHelper;
//
//import javax.crypto.KeyGenerator;
using Elastosx::Crypto::ISecretKey;
using Elastosx::Crypto::IKeyGenerator;
using Elastosx::Crypto::IKeyGeneratorHelper;
//TODO using Elastosx::Crypto::CKeyGeneratorHelper;
//import javax.crypto.SecretKey;
//
//import libcore.io.IoUtils;
using Elastos::Core::StringBuilder;
using Elastos::Core::StringUtils;
using Libcore::IO::IIoUtils;
using Libcore::IO::CIoUtils;
using Elastos::Utility::Logging::Logger;

namespace Elastos {
namespace Droid {
namespace KeyStore {
namespace Security {

CAR_INTERFACE_IMPL(SystemKeyStore, Object, ISystemKeyStore);

const String SystemKeyStore::SYSTEM_KEYSTORE_DIRECTORY("misc/systemkeys");
const String SystemKeyStore::KEY_FILE_EXTENSION(".sks");
AutoPtr<ISystemKeyStore> SystemKeyStore::mInstance;// = new SystemKeyStore();


ECode SystemKeyStore::constructor()
{
    return NOERROR;
}

AutoPtr<ISystemKeyStore> SystemKeyStore::GetInstance()
{
    if (mInstance == NULL) {
        mInstance = new SystemKeyStore();
    }
    return mInstance;
}

String SystemKeyStore::ToHexString(
    /* [in] */ ArrayOf<Byte>* keyData)
{
    if (keyData == NULL) {
        return String(NULL);
    }
    Int32 keyLen = keyData->GetLength();
    Int32 expectedStringLen = keyData->GetLength() * 2;
    AutoPtr<StringBuilder> sb = new StringBuilder(expectedStringLen);
    for (Int32 i = 0; i < keyLen; i++) {
        String hexStr = StringUtils::ToHexString((*keyData)[i] & 0x00FF);
        if (hexStr.GetLength() == 1) {
            hexStr = String("0") + hexStr;
        }
        sb->Append(hexStr);
    }
    return sb->ToString();
}

ECode SystemKeyStore::GenerateNewKeyHexString(
    /* [in] */ Int32 numBits,
    /* [in] */ const String& algName,
    /* [in] */ const String& keyName,
    /* [out] */ String* hex)
{
    VALIDATE_NOT_NULL(hex);
    AutoPtr<ArrayOf<Byte> > key;
    GenerateNewKey(numBits, algName, keyName, (ArrayOf<Byte>**)&key);
    *hex = ToHexString(key);
    return NOERROR;
}

ECode SystemKeyStore::GenerateNewKey(
    /* [in] */ Int32 numBits,
    /* [in] */ const String& algName,
    /* [in] */ const String& keyName,
    /* [out, callee] */ ArrayOf<Byte>** result)
{
    VALIDATE_NOT_NULL(result);
    // Check if key with similar name exists. If so, return null.
    AutoPtr<IFile> keyFile = GetKeyFile(keyName);
    Boolean exists;
    if (keyFile->Exists(&exists), exists) {
        //throw new IllegalArgumentException();
        Logger::E("SystemKeyStore", "GenerateNewKey, keyfile exists");
        assert(0);
        return E_ILLEGAL_ARGUMENT_EXCEPTION;
    }

    AutoPtr<IKeyGeneratorHelper> kgHelper;
    //TODO CKeyGeneratorHelper::AcquireSingleton((IKeyGeneratorHelper**)&kgHelper);
    AutoPtr<IKeyGenerator> skg;
    kgHelper->GetInstance(algName, (IKeyGenerator**)&skg);

    AutoPtr<ISecureRandomHelper> srHelper;
    CSecureRandomHelper::AcquireSingleton((ISecureRandomHelper**)&srHelper);
    AutoPtr<ISecureRandom> srng;
    srHelper->GetInstance(String("SHA1PRNG"), (ISecureRandom**)&srng);
    skg->Init(numBits, srng);

    AutoPtr<ISecretKey> sk;
    skg->GenerateKey((ISecretKey**)&sk);
    AutoPtr<ArrayOf<Byte> > retKey;
    IKey::Probe(sk)->GetEncoded((ArrayOf<Byte>**)&retKey);

    //try {
        // Store the key
        Boolean createNewFile;
        if (!(keyFile->CreateNewFile(&createNewFile), createNewFile)) {
            //throw new IllegalArgumentException();
            Logger::E("SystemKeyStore", "GenerateNewKey, CreateNewFile");
            assert(0);
            return E_ILLEGAL_ARGUMENT_EXCEPTION;
        }

        AutoPtr<IFileOutputStream> fos;
        CFileOutputStream::New(keyFile, (IFileOutputStream**)&fos);
        IOutputStream::Probe(fos)->Write(retKey);
        IFlushable::Probe(fos)->Flush();
        FileUtils::Sync(fos);
        ICloseable::Probe(fos)->Close();
        String name;
        keyFile->GetName(&name);
        FileUtils::SetPermissions(name, (FileUtils::sS_IRUSR | FileUtils::sS_IWUSR), -1, -1);
    //} catch (IOException ioe) {
    //    return null;
    //}
    *result = retKey;
    REFCOUNT_ADD(*result);
    return NOERROR;
}

AutoPtr<IFile> SystemKeyStore::GetKeyFile(
    /* [in] */ const String& keyName)
{
    AutoPtr<IFile> sysKeystoreDir;
    AutoPtr<IFile> dataDir = Environment::GetDataDirectory();
    CFile::New(dataDir, SYSTEM_KEYSTORE_DIRECTORY, (IFile**)&sysKeystoreDir);
    AutoPtr<IFile> keyFile;
    CFile::New(sysKeystoreDir, keyName + KEY_FILE_EXTENSION, (IFile**)&keyFile);
    return keyFile;
}

ECode SystemKeyStore::RetrieveKeyHexString(
    /* [in] */ const String& keyName,
    /* [out] */ String* hex)
{
    VALIDATE_NOT_NULL(hex);
    AutoPtr<ArrayOf<Byte> > key;
    RetrieveKey(keyName, (ArrayOf<Byte>**)&key);
    *hex = ToHexString(key);
    return NOERROR;
}

ECode SystemKeyStore::RetrieveKey(
    /* [in] */ const String& keyName,
    /* [out, callee] */ ArrayOf<Byte>** key)
{
    AutoPtr<IFile> keyFile = GetKeyFile(keyName);
    Boolean exists;
    if (!(keyFile->Exists(&exists), exists)) {
        *key = NULL;
    }
    AutoPtr<IIoUtils> ioutils;
    CIoUtils::AcquireSingleton((IIoUtils**)&ioutils);
    String fileStr;
    IObject::Probe(keyFile)->ToString(&fileStr);
    ioutils->ReadFileAsByteArray(fileStr, key);
    return NOERROR;
}

ECode SystemKeyStore::DeleteKey(
    /* [in] */ const String& keyName)
{
    // Get the file first.
    AutoPtr<IFile> keyFile = GetKeyFile(keyName);
    Boolean exists;
    if (!(keyFile->Exists(&exists), exists)) {
        //throw new IllegalArgumentException();
        Logger::E("SystemKeyStore", "DeleteKey");
        return E_ILLEGAL_ARGUMENT_EXCEPTION;
    }

    return keyFile->Delete();
}

}// namespace Elastos
}// namespace Droid
}// namespace KeyStore
}// namespace Security
