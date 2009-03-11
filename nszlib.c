/* 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1(the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis,WITHOUT WARRANTY OF ANY KIND,either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * Alternatively,the contents of this file may be used under the terms
 * of the GNU General Public License(the "GPL"),in which case the
 * provisions of GPL are applicable instead of those above.  If you wish
 * to allow use of your version of this file only under the terms of the
 * GPL and not to allow others to use your version of this file under the
 * License,indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by the GPL.
 * If you do not delete the provisions above,a recipient may use your
 * version of this file under either the License or the GPL.
 *
 * Author Vlad Seryakov vlad@crystalballinc.com
 * 
 */

/*
 * nszlib.c -- Zlib API module
 *
 *  ns_zlib usage:
 *
 *    ns_zlib compress data
 *      Returns compressed string
 *
 *    ns_zlib uncompress data
 *       Uncompresses previously compressed string
 *
 *    ns_zlib gzip data
 *      Returns compressed string in gzip format, string can be saved in
 *      a file with extension .gz and gzip will be able to uncompress it
 *
 *     ns_zlib gzipfile file
 *      Compresses the specified file, creating a file with the
 *      same name but a .gz suffix appened
 *
 *    ns_zlib gunzip file
 *       Uncompresses gzip file and returns text
 *
 *
 * Authors
 *
 *     Vlad Seryakov vlad@crystalballinc.com
 */

#define USE_TCL8X
#include "ns.h"

#include "zlib.h"

#define VERSION "1.1"

NS_EXPORT int Ns_ModuleVersion = 1;

static int NsZlibInterpInit(Tcl_Interp *, void *);
static int ZlibCmd(void *context, Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[]);

unsigned char *Ns_ZlibCompress(unsigned char *inbuf, unsigned long inlen, unsigned long *outlen);
unsigned char *Ns_ZlibUncompress(unsigned char *inbuf, unsigned long inlen, unsigned long *outlen);

NS_EXPORT int Ns_ModuleInit(char *hServer, char *hModule)
{
    Ns_Log(Notice, "nszlib: zlib module version %s started", VERSION);
    Ns_TclRegisterTrace(hServer, NsZlibInterpInit, 0, NS_TCL_TRACE_CREATE);
    return NS_OK;
}

static int NsZlibInterpInit(Tcl_Interp * interp, void *context)
{
    Tcl_CreateObjCommand(interp, "ns_zlib", ZlibCmd, NULL, NULL);
    return NS_OK;
}

static
int ZlibCmd(void *context, Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    int rc, inlen;
    unsigned char *inbuf, *outbuf;
    unsigned long outlen, crc;

    if (objc < 3) {
        Tcl_AppendResult(interp, "wrong # args: should be \"", Tcl_GetString(objv[0]), " command args\"", NULL);
        return TCL_ERROR;
    }

    if (!strcmp("compress", Tcl_GetString(objv[1]))) {
        inbuf = Tcl_GetByteArrayFromObj(objv[2], (int *) &inlen);
        if (!(outbuf = Ns_ZlibCompress(inbuf, inlen, &outlen))) {
            Tcl_AppendResult(interp, "nszlib: compress failed", 0);
            return TCL_ERROR;
        }
        Tcl_SetObjResult(interp, Tcl_NewByteArrayObj(outbuf, (int) outlen));
        ns_free(outbuf);
        return TCL_OK;
    }

    if (!strcmp("uncompress", Tcl_GetString(objv[1]))) {
        inbuf = Tcl_GetByteArrayFromObj(objv[2], (int *) &inlen);
        if (!(outbuf = Ns_ZlibUncompress(inbuf, inlen, &outlen))) {
            Tcl_AppendResult(interp, "nszlib: uncompress failed", 0);
            return TCL_ERROR;
        }
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outbuf, (int) outlen));
        ns_free(outbuf);
        return TCL_OK;
    }

    if (!strcmp("gzip", Tcl_GetString(objv[1]))) {
        inbuf = Tcl_GetByteArrayFromObj(objv[2], (int *) &inlen);
        outlen = inlen * 1.1 + 30;
        outbuf = ns_malloc(outlen);
        outlen = outlen - 16;
        rc = compress2(outbuf + 8, &outlen, inbuf, inlen, 3);
        if (rc != Z_OK) {
            sprintf(outbuf, "%d", rc);
            Tcl_AppendResult(interp, "nszlib: gzip failed ", outbuf, 0);
            ns_free(outbuf);
            return TCL_ERROR;
        }
        // Gzip header
        memcpy(outbuf, "\037\213\010\000\000\000\000\000\000\003", 10);
        crc = crc32(0, Z_NULL, 0);
        crc = crc32(crc, inbuf, inlen);
        outlen += 4;
        *(unsigned char *) (outbuf + outlen + 0) = (crc & 0x000000ff);
        *(unsigned char *) (outbuf + outlen + 1) = (crc & 0x0000ff00) >> 8;
        *(unsigned char *) (outbuf + outlen + 2) = (crc & 0x00ff0000) >> 16;
        *(unsigned char *) (outbuf + outlen + 3) = (crc & 0xff000000) >> 24;
        *(unsigned char *) (outbuf + outlen + 4) = (inlen & 0x000000ff);
        *(unsigned char *) (outbuf + outlen + 5) = (inlen & 0x0000ff00) >> 8;
        *(unsigned char *) (outbuf + outlen + 6) = (inlen & 0x00ff0000) >> 16;
        *(unsigned char *) (outbuf + outlen + 7) = (inlen & 0xff000000) >> 24;
        outlen += 8;
        Tcl_SetObjResult(interp, Tcl_NewByteArrayObj(outbuf, (int) outlen));
        ns_free(outbuf);
        return TCL_OK;
    }

    if (!strcmp("gunzip", Tcl_GetString(objv[1]))) {
        Tcl_Obj *obj;
        char buf[32768];
        gzFile gin = gzopen(Tcl_GetString(objv[2]), "rb");
        if (!gin) {
            Tcl_AppendResult(interp, "nszlib: gunzip: cannot open ", Tcl_GetString(objv[2]), 0);
            return TCL_ERROR;
        }
        obj = Tcl_NewStringObj(0, 0);
        for (;;) {
            if (!(inlen = gzread(gin, buf, sizeof(buf))))
                break;
            if (inlen < 0) {
                Tcl_AppendResult(interp, "nszlib: gunzip: read error ", gzerror(gin, &rc), 0);
                Tcl_DecrRefCount(obj);
                gzclose(gin);
                return TCL_ERROR;
            }
            Tcl_AppendToObj(obj, buf, (int) inlen);
        }
        Tcl_SetObjResult(interp, obj);
        gzclose(gin);
        return TCL_OK;
    }

    if (!strcmp("gzipfile", Tcl_GetString(objv[1]))) {
        FILE *fin;
        gzFile gout;
        Tcl_Obj *obj;
        char buf[32768];

        if (!(fin = fopen(Tcl_GetString(objv[2]), "rb"))) {
            Tcl_AppendResult(interp, "nszlib: gzipfile: cannot open ", Tcl_GetString(objv[2]), 0);
            return TCL_ERROR;
        }
        obj = Tcl_NewStringObj(Tcl_GetString(objv[2]), -1);
        Tcl_AppendToObj(obj, ".gz", 3);

        if (!(gout = gzopen(Tcl_GetString(obj), "wb"))) {
            Tcl_AppendResult(interp, "nszlib: gzipfile: cannot create ", Tcl_GetString(obj), 0);
            Tcl_DecrRefCount(obj);
            return TCL_ERROR;
        }
        for (;;) {
            if (!(inlen = fread(buf, 1, sizeof(buf), fin)))
                break;
            if (ferror(fin)) {
                Tcl_AppendResult(interp, "nszlib: gzipfile: read error ", strerror(errno), 0);
                fclose(fin);
                gzclose(gout);
                unlink(Tcl_GetString(objv[2]));
                Tcl_DecrRefCount(obj);
                return TCL_ERROR;
            }
            if (gzwrite(gout, buf, inlen) != inlen) {
                Tcl_AppendResult(interp, "nszlib: gunzip: write error ", gzerror(gout, &rc), 0);
                fclose(fin);
                gzclose(gout);
                unlink(Tcl_GetString(objv[2]));
                Tcl_DecrRefCount(obj);
                return TCL_ERROR;
            }
        }
        fclose(fin);
        gzclose(gout);
        unlink(Tcl_GetString(objv[2]));
        Tcl_SetObjResult(interp, obj);
        return TCL_OK;
    }
    Tcl_AppendResult(interp, "nszlib: unknown command: should be one of ", "compress,uncompress,gzip,gunzip", 0);
    return TCL_ERROR;
}

unsigned char *Ns_ZlibCompress(unsigned char *inbuf, unsigned long inlen, unsigned long *outlen)
{
    int rc;
    unsigned long crc;
    unsigned char *outbuf;

    *outlen = inlen * 1.1 + 20;
    outbuf = ns_malloc(*outlen);
    *outlen = (*outlen) - 8;
    rc = compress2(outbuf, outlen, inbuf, inlen, 3);
    if (rc != Z_OK) {
        Ns_Log(Error, "Ns_ZlibCompress: error %d", rc);
        ns_free(outbuf);
        return 0;
    }
    crc = crc32(crc32(0, Z_NULL, 0), inbuf, inlen);
    crc = htonl(crc);
    inlen = htonl(inlen);
    memcpy(outbuf + (*outlen), &crc, 4);
    memcpy(outbuf + (*outlen) + 4, &inlen, 4);
    (*outlen) += 8;
    return outbuf;
}

unsigned char *Ns_ZlibUncompress(unsigned char *inbuf, unsigned long inlen, unsigned long *outlen)
{
    int rc;
    unsigned long crc;
    unsigned char *outbuf;

    memcpy(outlen, &inbuf[inlen - 4], 4);
    *outlen = ntohl(*outlen);
    outbuf = ns_malloc((*outlen) + 1);
    rc = uncompress(outbuf, outlen, inbuf, inlen - 8);
    if (rc != Z_OK) {
        Ns_Log(Error, "Ns_ZlibUncompress: error %d", rc);
        ns_free(outbuf);
        return 0;
    }
    memcpy(&crc, &inbuf[inlen - 8], 4);
    crc = ntohl(crc);
    if (crc != crc32(crc32(0, Z_NULL, 0), outbuf, *outlen)) {
        Ns_Log(Error, "Ns_ZlibUncompress: crc mismatch");
        ns_free(outbuf);
        return 0;
    }
    return outbuf;
}
