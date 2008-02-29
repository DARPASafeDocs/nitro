/* =========================================================================
 * This file is part of NITRO
 * =========================================================================
 * 
 * (C) Copyright 2004 - 2008, General Dynamics - Advanced Information Systems
 *
 * NITRO is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public 
 * License along with this program; if not, If not, 
 * see <http://www.gnu.org/licenses/>.
 *
 */

#include "nitf_Field.h"
#include "nitf_JNI.h"

NITF_JNI_DECLARE_OBJ(nitf_Field)
/*
 * Class:     nitf_Field
 * Method:    getType
 * Signature: ()Lnitf/FieldType;
 */
JNIEXPORT jobject JNICALL Java_nitf_Field_getType
    (JNIEnv * env, jobject self)
{
    nitf_Field *field = _GetObj(env, self);

    jobject enumBCSA, enumBCSN, enumBINARY;

    jclass fieldTypeClass = (*env)->FindClass(env, "nitf/FieldType");
    jfieldID fieldID;

    /* BCS-A */
    fieldID =
        (*env)->GetStaticFieldID(env, fieldTypeClass, "NITF_BCS_A",
                                 "Lnitf/FieldType;");
    enumBCSA = (*env)->GetStaticObjectField(env, fieldTypeClass, fieldID);

    /* BCS-N */
    fieldID =
        (*env)->GetStaticFieldID(env, fieldTypeClass, "NITF_BCS_N",
                                 "Lnitf/FieldType;");
    enumBCSN = (*env)->GetStaticObjectField(env, fieldTypeClass, fieldID);

    /* BINARY */
    fieldID =
        (*env)->GetStaticFieldID(env, fieldTypeClass, "NITF_BINARY",
                                 "Lnitf/FieldType;");
    enumBINARY =
        (*env)->GetStaticObjectField(env, fieldTypeClass, fieldID);

    switch (field->type)
    {
    case NITF_BCS_A:
        return enumBCSA;
    case NITF_BCS_N:
        return enumBCSN;
    case NITF_BINARY:
        return enumBINARY;
    default:
        return NULL;
    }
}


/*
 * Class:     nitf_Field
 * Method:    getLength
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_nitf_Field_getLength
    (JNIEnv * env, jobject self)
{
    nitf_Field *field = _GetObj(env, self);
    return field->length;
}


/*
 * Class:     nitf_Field
 * Method:    setRawData
 * Signature: ([B)Z
 */
JNIEXPORT jboolean JNICALL Java_nitf_Field_setRawData
    (JNIEnv * env, jobject self, jbyteArray data)
{
    nitf_Field *field = _GetObj(env, self);
    nitf_Error error;

    jbyte *tmp;
    jint length = (*env)->GetArrayLength(env, data);
    tmp = (*env)->GetByteArrayElements(env, data, 0);

    if (!nitf_Field_setRawData(field, (NITF_DATA *) tmp, length, &error))
    {
        return JNI_FALSE;
    }
    return JNI_TRUE;
}


/*
 * Class:     nitf_Field
 * Method:    getRawData
 * Signature: ()[B
 */
JNIEXPORT jbyteArray JNICALL Java_nitf_Field_getRawData
    (JNIEnv * env, jobject self)
{
    nitf_Field *field = _GetObj(env, self);
    jbyteArray byteArray = (*env)->NewByteArray(env, field->length);
    (*env)->SetByteArrayRegion(env, byteArray, 0, field->length,
                               field->raw);
    return byteArray;
}


/*
 * Class:     nitf_Field
 * Method:    getStringData
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_nitf_Field_getStringData
    (JNIEnv * env, jobject self)
{
    nitf_Field *field = _GetObj(env, self);
    jchar *buf;
    nitf_Error error;
    jstring string;

    /* create buffer to copy data to */
    buf = (jchar *) NITF_MALLOC(field->length + 1);
    if (!buf)
        return NULL;

    memset(buf, 0, field->length + 1);

    /* TODO: check for an error here */
    if (!nitf_Field_get
        (field, buf, NITF_CONV_STRING, field->length + 1, &error))
    {
        NITF_FREE(buf);
        return NULL;
    }

    /* get the string */
    string = (*env)->NewStringUTF(env, buf);

    /* free buf */
    NITF_FREE(buf);
    return string;
}


/*
 * Class:     nitf_Field
 * Method:    getIntData
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_nitf_Field_getIntData
    (JNIEnv * env, jobject self)
{
    nitf_Field *field = _GetObj(env, self);
    jint intData;
    nitf_Error error;

    nitf_Field_get(field, &intData, NITF_CONV_INT, sizeof(intData),
                   &error);
    /* TODO: deal with errors */
    return intData;
}

