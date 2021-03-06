//=========================================================================
// Copyright (C) 2012 The Elastos Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//=========================================================================

#include "Elastos.CoreLibrary.Extensions.h"
#include "Elastos.CoreLibrary.Utility.h"
#include "Elastos.CoreLibrary.Security.h"
#include "_Org.Conscrypt.h"
#include "elastos/droid/keystore/security/ElastosKeyPairGenerator.h"
#include "elastos/droid/keystore/security/KeyStore.h"
#include "elastos/droid/keystore/security/Credentials.h"
#include "elastos/core/CoreUtils.h"
#include <elastos/utility/logging/Logger.h>

//import com.android.org.bouncycastle.x509.X509V3CertificateGenerator;
using Org::Conscrypt::INativeCrypto;
using Org::Conscrypt::IOpenSSLEngine;
using Org::Conscrypt::IOpenSSLEngineHelper;
using Org::Conscrypt::COpenSSLEngineHelper;
using Org::Conscrypt::EIID_IOpenSSLEngineHelper;
using Org::Conscrypt::IOpenSSLX509V3CertificateGeneratorHelper;
using Org::Conscrypt::COpenSSLX509V3CertificateGeneratorHelper;
using Elastos::Math::IBigInteger;
using Elastos::Security::IPrincipal;
using Elastos::Security::IKeyPair;
using Elastos::Security::CKeyPair;
using Elastos::Security::IPrivateKey;
using Elastos::Security::IPublicKey;
using Elastos::Security::IKeyFactory;
using Elastos::Security::IKeyFactoryHelper;
using Elastos::Security::CKeyFactoryHelper;
using Elastos::Security::ISecureRandom;
using Elastos::Security::Cert::ICertificate;
using Elastos::Security::Cert::IX509Certificate;
using Elastos::Security::Spec::IRSAKeyGenParameterSpec;
using Elastos::Security::Spec::IDSAParameterSpec;
using Elastos::Security::Interfaces::IDSAParams;
using Elastos::Security::Spec::IKeySpec;
using Elastos::Security::Spec::IX509EncodedKeySpec;
using Elastos::Security::Spec::CX509EncodedKeySpec;
using Elastos::Core::CoreUtils;
using Elastos::Utility::IDate;
using Elastos::Utility::Logging::Logger;
using Elastosx::Security::Auth::X500::IX500Principal;

namespace Elastos {
namespace Droid {
namespace KeyStore {
namespace Security {

CAR_INTERFACE_IMPL(ElastosKeyPairGenerator, KeyPairGeneratorSpi, IElastosKeyPairGenerator);

ECode ElastosKeyPairGenerator::constructor()
{
    return NOERROR;
}

ECode ElastosKeyPairGenerator::GenerateKeyPair(
    /* [out] */ IKeyPair** keyPair)
{
    VALIDATE_NOT_NULL(keyPair);
    *keyPair = NULL;

    if (mKeyStore == NULL || mSpec == NULL) {
        //throw new IllegalStateException("Must call initialize with an android.security.KeyPairGeneratorSpec first");
        Logger::E("ElastosKeyPairGenerator", "Must call initialize with an android.security.KeyPairGeneratorSpec first");
        return E_ILLEGAL_STATE_EXCEPTION;
    }

    Int32 flags;
    KeyStoreState state;
    if ((((mSpec->GetFlags(&flags), flags) & KeyStore::FLAG_ENCRYPTED) != 0) && ((mKeyStore->State(&state), state) != KeyStoreState_UNLOCKED)) {
        //throw new IllegalStateException( "Android keystore must be in initialized and unlocked state if encryption is required");
        Logger::E("ElastosKeyPairGenerator", "Android keystore must be in initialized and unlocked state if encryption is required");
        return E_ILLEGAL_STATE_EXCEPTION;
    }

    String alias;
    mSpec->GetKeystoreAlias(&alias);

    Boolean bTmp;
    Credentials::DeleteAllTypesForAlias(mKeyStore, alias, &bTmp);

    String skeyType;
    mSpec->GetKeyType(&skeyType);
    Int32 keyType = KeyStore::GetKeyTypeForAlgorithm(skeyType);
    AutoPtr<IAlgorithmParameterSpec> aps;
    mSpec->GetAlgorithmParameterSpec((IAlgorithmParameterSpec**)&aps);
    AutoPtr<ArrayOf<ByteArray > > args = GetArgsForKeyType(keyType, aps);

    String privateKeyAlias = Credentials::USER_PRIVATE_KEY + alias;
    Int32 keySize;
    mSpec->GetKeySize(&keySize);
    AutoPtr<ArrayOf<IArrayOf*> > args2;
    if (args != NULL) {
        Int32 argsLen = args->GetLength();
        args2 = ArrayOf<IArrayOf*>::Alloc(argsLen);
        for (Int32 i = 0; i < argsLen; ++i) {
            args2->Set(i, CoreUtils::ConvertByteArray((*args)[i]));
        }
    }

    mSpec->GetFlags(&flags);
    if (!(mKeyStore->Generate(privateKeyAlias, KeyStore::UID_SELF, keyType,
            keySize, flags, args2, &bTmp), bTmp)) {
        //throw new IllegalStateException("could not generate key in keystore");
        Logger::E("ElastosKeyPairGenerator", "could not generate key in keystore");
        return E_ILLEGAL_STATE_EXCEPTION;
    }

    AutoPtr<IPrivateKey> privKey;
    AutoPtr<IOpenSSLEngineHelper> opensslEngineHelper;
    COpenSSLEngineHelper::AcquireSingleton((IOpenSSLEngineHelper**)&opensslEngineHelper);
    AutoPtr<IOpenSSLEngine> engine;
    opensslEngineHelper->GetInstance(String("keystore"), (IOpenSSLEngine**)&engine);

    //try {
    ECode ec = engine->GetPrivateKeyById(privateKeyAlias, (IPrivateKey**)&privKey);
    Logger::E("ElastosKeyPairGenerator", "=====[snow]===== privKey=%s", TO_CSTR(privKey));
    //} catch (InvalidKeyException e) {
    if (ec == (ECode)E_INVALID_KEY_EXCEPTION) {
        //throw new RuntimeException("Can't get key", e);
        Logger::E("ElastosKeyPairGenerator", "Can't get key");
        return E_INVALID_KEY_EXCEPTION;
    }

    AutoPtr<ArrayOf<Byte> > pubKeyBytes;
    mKeyStore->GetPubkey(privateKeyAlias, (ArrayOf<Byte>**)&pubKeyBytes);

    AutoPtr<IPublicKey> pubKey;
    //try
    {
        AutoPtr<IKeyFactoryHelper> kfHelper;
        CKeyFactoryHelper::AcquireSingleton((IKeyFactoryHelper**)&kfHelper);
        FAIL_GOTO(ec = mSpec->GetKeyType(&skeyType), ERROR)
        AutoPtr<IKeyFactory> keyFact;
        FAIL_GOTO(ec = kfHelper->GetInstance(skeyType, (IKeyFactory**)&keyFact), ERROR)
        AutoPtr<IX509EncodedKeySpec> x509eks;
        FAIL_GOTO(ec = CX509EncodedKeySpec::New(pubKeyBytes, (IX509EncodedKeySpec**)&x509eks), ERROR)
        FAIL_GOTO(ec = keyFact->GeneratePublic(IKeySpec::Probe(x509eks), (IPublicKey**)&pubKey), ERROR)
        Logger::E("ElastosKeyPairGenerator", "=====[snow]===== pubKey=%s", TO_CSTR(pubKey));
    }
    // catch (NoSuchAlgorithmException e) {
ERROR:
    if (ec == (ECode)E_NO_SUCH_ALGORITHM_EXCEPTION) {
        //throw new IllegalStateException("Can't instantiate key generator", e);
        Logger::E("ElastosKeyPairGenerator", "Can't instantiate key generator");
        return E_ILLEGAL_STATE_EXCEPTION;
    }
    //} catch (InvalidKeySpecException e) {
    if (ec == (ECode)E_INVALID_KEY_SPEC_EXCEPTION) {
        //throw new IllegalStateException("keystore returned invalid key encoding", e);
        Logger::E("ElastosKeyPairGenerator", "keystore returned invalid key encoding");
        return E_ILLEGAL_STATE_EXCEPTION;
    }

    Logger::E("ElastosKeyPairGenerator", "=====[snow]=====TODO: need class X509V3CertificateGenerator from lib:bouncycastle");
    AutoPtr<IOpenSSLX509V3CertificateGeneratorHelper> certGenHelper;
    COpenSSLX509V3CertificateGeneratorHelper::AcquireSingleton((IOpenSSLX509V3CertificateGeneratorHelper**)&certGenHelper);
    //AutoPtr<IX509V3CertificateGenerator> certGen;
    //CX509V3CertificateGenerator::New((IX509V3CertificateGenerator**)&certGen);
    AutoPtr<IBigInteger> serialNumber;
    mSpec->GetSerialNumber((IBigInteger**)&serialNumber);
    AutoPtr<IX500Principal> subDN;
    mSpec->GetSubjectDN((IX500Principal**)&subDN);
    String subjectDNName;
    IPrincipal::Probe(subDN)->GetName(&subjectDNName);

    AutoPtr<IDate> start, end;
    mSpec->GetStartDate((IDate**)&start);
    mSpec->GetEndDate((IDate**)&end);
    String sigAl;
    GetDefaultSignatureAlgorithmForKeyType(skeyType, &sigAl);

    AutoPtr<IX509Certificate> cert;
    //try {
    ec = certGenHelper->Generate(pubKey, privKey, serialNumber, start, end,
                subjectDNName, String(NULL), (IX509Certificate**)&cert);
    //} catch (Exception e) {
    if (ec == (ECode)E_CERTIFICATE_ENCODING_EXCEPTION) {
        //    Credentials.deleteAllTypesForAlias(mKeyStore, alias);
        //    throw new IllegalStateException("Can't generate certificate", e);
        Boolean bTmp;
        Credentials::DeleteAllTypesForAlias(mKeyStore, alias, &bTmp);
        Logger::E("ElastosKeyPairGenerator", "Can't generate certificate");
        return E_ILLEGAL_STATE_EXCEPTION;
    }

    AutoPtr<ArrayOf<Byte> > certBytes;
    // //try {
    ec = ICertificate::Probe(cert)->GetEncoded((ArrayOf<Byte>**)&certBytes);
    // //} catch (CertificateEncodingException e) {
    if (ec == (ECode)E_CERTIFICATE_ENCODING_EXCEPTION) {
        Boolean bTmp;
        Credentials::DeleteAllTypesForAlias(mKeyStore, alias, &bTmp);
        //throw new IllegalStateException("Can't get encoding of certificate", e);
        Logger::E("ElastosKeyPairGenerator", "Can't get encoding of certificate");
        return E_ILLEGAL_STATE_EXCEPTION;
    }

    mSpec->GetFlags(&flags);
    Boolean put;
    if (!(mKeyStore->Put(Credentials::USER_CERTIFICATE + alias, certBytes, KeyStore::UID_SELF, flags, &put), put)) {
        Boolean bTmp;
        Credentials::DeleteAllTypesForAlias(mKeyStore, alias, &bTmp);
        //throw new IllegalStateException("Can't store certificate in AndroidKeyStore");
        Logger::E("ElastosKeyPairGenerator", "Can't store certificate in AndroidKeyStore");
        return E_ILLEGAL_STATE_EXCEPTION;
    }

    return CKeyPair::New(pubKey, privKey, keyPair);
}

ECode ElastosKeyPairGenerator::GetDefaultSignatureAlgorithmForKeyType(
    /* [in] */ const String& keyType,
    /* [out] */ String* result)
{
    VALIDATE_NOT_NULL(result);
    if (String("RSA").EqualsIgnoreCase(keyType)) {
        *result = String("sha256WithRSA");
    } else if (String("DSA").EqualsIgnoreCase(keyType)) {
        *result = String("sha1WithDSA");
    } else if (String("EC").EqualsIgnoreCase(keyType)) {
        *result = String("sha256WithECDSA");
    } else {
        //throw new IllegalArgumentException("Unsupported key type " + keyType);
        Logger::E("ElastosKeyPairGenerator", "Unsupported key type %s", keyType.string());
        return E_ILLEGAL_ARGUMENT_EXCEPTION;
    }
    return NOERROR;
}

AutoPtr< ArrayOf<ByteArray  > > ElastosKeyPairGenerator::GetArgsForKeyType(
    /* [in] */ Int32 keyType,
    /* [in] */ IAlgorithmParameterSpec* spec)
{
    switch (keyType) {
        case INativeCrypto::EVP_PKEY_RSA:
            if (IRSAKeyGenParameterSpec::Probe(spec) != NULL) {
                IRSAKeyGenParameterSpec* rsaSpec = IRSAKeyGenParameterSpec::Probe(spec);
                AutoPtr<IBigInteger> bi;
                rsaSpec->GetPublicExponent((IBigInteger**)&bi);
                AutoPtr<ArrayOf<Byte> > array;
                bi->ToByteArray((ArrayOf<Byte>**)&array);
                AutoPtr<ArrayOf<ByteArray > > result = ArrayOf<ByteArray >::Alloc(1);
                result->Set(0, array);
                return result;
            }
            break;
        case INativeCrypto::EVP_PKEY_DSA:
            if (IDSAParameterSpec::Probe(spec) != NULL) {
                IDSAParameterSpec* dsaSpec = IDSAParameterSpec::Probe(spec);
                IDSAParams* dsaParams = IDSAParams::Probe(dsaSpec);

                AutoPtr<IBigInteger> bi1;
                dsaParams->GetG((IBigInteger**)&bi1);
                AutoPtr<ArrayOf<Byte> > array1;
                bi1->ToByteArray((ArrayOf<Byte>**)&array1);

                AutoPtr<IBigInteger> bi2;
                dsaParams->GetP((IBigInteger**)&bi2);
                AutoPtr<ArrayOf<Byte> > array2;
                bi2->ToByteArray((ArrayOf<Byte>**)&array2);

                AutoPtr<IBigInteger> bi3;
                dsaParams->GetQ((IBigInteger**)&bi3);
                AutoPtr<ArrayOf<Byte> > array3;
                bi3->ToByteArray((ArrayOf<Byte>**)&array3);

                AutoPtr<ArrayOf<ByteArray > > result = ArrayOf<ByteArray >::Alloc(3);
                result->Set(0, array1);
                result->Set(1, array2);
                result->Set(2, array3);
                return result;
            }
            break;
    }
    return NULL;
}

ECode ElastosKeyPairGenerator::Initialize(
    /* [in] */ Int32 keysize,
    /* [in] */ ISecureRandom* random)
{
    //throw new IllegalArgumentException("cannot specify keysize with AndroidKeyPairGenerator");
    Logger::E("ElastosKeyPairGenerator", "Initialize - cannot specify keysize with AndroidKeyPairGenerator");
    return NOERROR;
}

ECode ElastosKeyPairGenerator::Initialize(
    /* [in] */ IAlgorithmParameterSpec* params,
    /* [in] */ ISecureRandom* random)
{
    if (params == NULL) {
        //throw new InvalidAlgorithmParameterException(
        //        "must supply params of type android.security.KeyPairGeneratorSpec");
        Logger::E("ElastosKeyPairGenerator", "Initialize - must supply params of type android.security.KeyPairGeneratorSpec");
        return E_ILLEGAL_ARGUMENT_EXCEPTION;
    }
    else if (IKeyPairGeneratorSpec::Probe(params) == NULL) {
        //throw new InvalidAlgorithmParameterException(
        //        "params must be of type android.security.KeyPairGeneratorSpec");
        Logger::E("ElastosKeyPairGenerator", "Initialize - params must be of type android.security.KeyPairGeneratorSpec");
        return E_ILLEGAL_ARGUMENT_EXCEPTION;
    }

    mSpec = IKeyPairGeneratorSpec::Probe(params);

    mKeyStore = KeyStore::GetInstance();
    return NOERROR;
}

}// namespace Elastos
}// namespace Droid
}// namespace KeyStore
}// namespace Security
