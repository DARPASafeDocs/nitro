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

#include "nitf_Extensions.h"
#include "nitf_JNI.h"

NITF_JNI_DECLARE_OBJ(nitf_Extensions)
/*
 * Class:     nitf_Extensions
 * Method:    appendTRE
 * Signature: (Lnitf/TRE;)V
 */
JNIEXPORT void JNICALL Java_nitf_Extensions_appendTRE
  (JNIEnv *env, jobject self, jobject treObject)
{
    nitf_Extensions *extensions = _GetObj(env, self);
    jclass treClass = (*env)->FindClass(env, "nitf/TRE");
    jclass exClass = (*env)->FindClass(env, "nitf/NITFException");
    nitf_Error error;
    jmethodID methodID =
        (*env)->GetMethodID(env, treClass, "getAddress", "()J");
    nitf_TRE *tre =
        (nitf_TRE *) (*env)->CallLongMethod(env, treObject, methodID);

    if (!nitf_Extensions_appendTRE(extensions, tre, &error))
    {
        (*env)->ThrowNew(env, exClass, error.message);
    }
}


/*
 * Class:     nitf_Extensions
 * Method:    getTREsByName
 * Signature: (Ljava/lang/String;)Ljava/lang/Iterable;
 */
JNIEXPORT jobject JNICALL Java_nitf_Extensions_getTREsByName
  (JNIEnv *env, jobject self, jstring name)
{
    nitf_Extensions *extensions = _GetObj(env, self);
    jclass treClass = (*env)->FindClass(env, "nitf/TRE");
    jclass vecClass = (*env)->FindClass(env, "java/util/Vector");
    jmethodID methodID =
        (*env)->GetMethodID(env, treClass, "<init>", "(J)V");
    jmethodID vecAddMethodID =
        (*env)->GetMethodID(env, vecClass, "add", "(Ljava/lang/Object;)Z");
    nitf_ListIterator iter, end;
    nitf_TRE *tre;
    jobject treObject;
    char *tmp = (*env)->GetStringUTFChars(env, name, 0);
    nitf_List* list;
    jobject vector = NULL;
    
    vector = (*env)->NewObject(env, vecClass,
        (*env)->GetMethodID(env, vecClass, "<init>", "()V"));
    
    if (extensions != NULL)
    {
        /* get the list */
        list = nitf_Extensions_getTREsByName(extensions, tmp);
        
        /* set up iterators */
        iter = nitf_List_begin(list);
        end = nitf_List_end(list);

        while (nitf_ListIterator_notEqualTo(&iter, &end))
        {
            tre = (nitf_TRE *) nitf_ListIterator_get(&iter);

            /* get the object and add it to the array */
            treObject =
                (*env)->NewObject(env, treClass, methodID, (jlong) tre);
            (*env)->CallBooleanMethod(env, vector, vecAddMethodID, treObject);
            nitf_ListIterator_increment(&iter);
        }
    }
    return vector;
}


/*
 * Class:     nitf_Extensions
 * Method:    exists
 * Signature: (Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_nitf_Extensions_exists
    (JNIEnv * env, jobject self, jstring name)
{
    nitf_Extensions *extensions = _GetObj(env, self);
    char *tmp;

    if (extensions)
    {
        tmp = (*env)->GetStringUTFChars(env, name, 0);
        return nitf_Extensions_exists(extensions,
                                      tmp) ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}


/*
 * Class:     nitf_Extensions
 * Method:    removeTREsByName
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_nitf_Extensions_removeTREsByName
  (JNIEnv *env, jobject self, jstring name)
{
    nitf_Extensions *extensions = _GetObj(env, self);
    char *tmp;

    if (extensions)
    {
        tmp = (char *) (*env)->GetStringUTFChars(env, name, 0);
        nitf_Extensions_removeTREsByName(extensions, tmp);
    }
}


/*
 * Class:     nitf_Extensions
 * Method:    getAll
 * Signature: ()Ljava/lang/Iterable;
 */
JNIEXPORT jobject JNICALL Java_nitf_Extensions_getAll
  (JNIEnv *env, jobject self)
{
    nitf_Extensions *extensions = _GetObj(env, self);
    nitf_ExtensionsIterator startIter, endIter;
    jclass treClass = (*env)->FindClass(env, "nitf/TRE");
    jclass vecClass = (*env)->FindClass(env, "java/util/Vector");
    jmethodID vecAddMethodID =
        (*env)->GetMethodID(env, vecClass, "add", "(Ljava/lang/Object;)Z");
    jmethodID methodID = (*env)->GetMethodID(env, treClass, "<init>", "(J)V");
    nitf_TRE *tre;
    jobject element;
    jobject vector;
    
    vector = (*env)->NewObject(env, vecClass,
        (*env)->GetMethodID(env, vecClass, "<init>", "()V"));

    if (extensions)
    {
        startIter = nitf_Extensions_begin(extensions);
        endIter = nitf_Extensions_end(extensions);

        /* now, iterate and add to the array */
        while (nitf_ExtensionsIterator_notEqualTo(&startIter, &endIter))
        {
            tre = nitf_ExtensionsIterator_get(&startIter);
            element = (*env)->NewObject(env,
                                        treClass, methodID, (jlong) tre);
            (*env)->CallBooleanMethod(env, vector, vecAddMethodID, element);
            nitf_ExtensionsIterator_increment(&startIter);
        }
    }
    return vector;
}


/*
 * Class:     nitf_Extensions
 * Method:    computeLength
 * Signature: (Lnitf/Version;)J
 */
JNIEXPORT jlong JNICALL Java_nitf_Extensions_computeLength
    (JNIEnv * env, jobject self, jobject versionObject)
{
    nitf_Extensions *extensions = _GetObj(env, self);
    nitf_Error error;
    nitf_Version version = _GetNITFVersion(env, versionObject);

    return (jlong) nitf_Extensions_computeLength(extensions, version,
                                                 &error);
}

