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

#define NSZLIB_VERSION "1.1"

NS_EXPORT int Ns_ModuleVersion = 1;
NS_EXPORT Ns_ModuleInitProc Ns_ModuleInit;

static Ns_TclTraceProc NsZlibInterpInit;
static Tcl_ObjCmdProc ZlibCmd;

unsigned char *Ns_ZlibCompress(unsigned char *inbuf, unsigned long inlen, unsigned long *outlen);
unsigned char *Ns_ZlibUncompress(unsigned char *inbuf, unsigned long inlen, unsigned long *outlen);
unsigned char *Ns_ZlibDeflate(unsigned char *inbuf, unsigned long inlen, unsigned long *outlen);
unsigned char *Ns_ZlibInflate(unsigned char *inbuf, unsigned long inlen, unsigned long *outlen);

NS_EXPORT int Ns_ModuleInit(const char *hServer, const char *hModule)
{
    Ns_Log(Notice, "nszlib: zlib module version %s started", NSZLIB_VERSION);
    Ns_TclRegisterTrace(hServer, NsZlibInterpInit, 0, NS_TCL_TRACE_CREATE);
    return NS_OK;
}

static int NsZlibInterpInit(Tcl_Interp * interp, const void *context)
{
    Tcl_CreateObjCommand(interp, "ns_zlib", ZlibCmd, NULL, NULL);
    return NS_OK;
}

static
int ZlibCmd(void *context, Tcl_Interp * interp, int objc, Tcl_Obj *CONST* objv)
{
    int result = TCL_OK, inlen, opt, rc;
    unsigned char *inbuf, *outbuf = NULL;
    unsigned long outlen;

    static const char *opts[] = {
        "compress", "deflate", "gzip", "gzipfile",
        "gunzip", "inflate", "uncompress"
    };
    enum {
        CCompressIdx, CDeflateIdx, CGzipIdx, CGzipfileIdx,
        CGunzipIdx, CInflateIdx, CUncompressIdx
    };

    if (Tcl_GetIndexFromObj(interp, objv[1], opts, 
                            "option", 0, &opt) != TCL_OK) {
        return TCL_ERROR;
    }
    
    if (objc < 3) {
        Tcl_AppendResult(interp, "wrong # args: should be \"", Tcl_GetString(objv[0]), " command args\"", NULL);
        return TCL_ERROR;
    }

    switch (opt) {

    case CCompressIdx:
        inbuf = Tcl_GetByteArrayFromObj(objv[2], (int *) &inlen);
        outbuf = Ns_ZlibCompress(inbuf, inlen, &outlen);
        if (outbuf == NULL) {
            Tcl_AppendResult(interp, "nszlib: compress failed", 0);
            result = TCL_ERROR;
        } else {
            Tcl_SetObjResult(interp, Tcl_NewByteArrayObj(outbuf, (int) outlen));
        }
        break;

    case CDeflateIdx:
        inbuf = Tcl_GetByteArrayFromObj(objv[2], (int *) &inlen);
        outbuf = Ns_ZlibDeflate(inbuf, inlen, &outlen);
        if (outbuf == NULL) {
            Tcl_AppendResult(interp, "nszlib: deflate failed", 0);
            result = TCL_ERROR;
        } else {
            Tcl_SetObjResult(interp, Tcl_NewByteArrayObj(outbuf, (int) outlen));
        }
        break;
        
    case CUncompressIdx:
        inbuf = Tcl_GetByteArrayFromObj(objv[2], (int *) &inlen);
        outbuf = Ns_ZlibUncompress(inbuf, inlen, &outlen);
        if (outbuf == NULL) {
            Tcl_AppendResult(interp, "nszlib: uncompress failed", 0);
            result = TCL_ERROR;
        } else {
            Tcl_SetObjResult(interp, Tcl_NewStringObj((char*)outbuf, (int) outlen));
        }
        break;

    case CInflateIdx:
        inbuf = Tcl_GetByteArrayFromObj(objv[2], (int *) &inlen);
        outbuf = Ns_ZlibInflate(inbuf, inlen, &outlen);
        if (outbuf == NULL) {
            Tcl_AppendResult(interp, "nszlib: inflate failed", 0);
            result = TCL_ERROR;
        } else {
            Tcl_SetObjResult(interp, Tcl_NewByteArrayObj(outbuf, (int) outlen));
        }
        break;

        
    case CGzipIdx:
        inbuf = Tcl_GetByteArrayFromObj(objv[2], (int *) &inlen);
        outlen = inlen * 1.1 + 30;
        outbuf = ns_malloc(outlen);
        outlen = outlen - 16;
        rc = compress2(outbuf + 8, &outlen, inbuf, inlen, 3);
        if (rc != Z_OK) {
	    sprintf((char *) outbuf, "%d", rc);
            Tcl_AppendResult(interp, "nszlib: gzip failed ", outbuf, 0);
            result = TCL_ERROR;
        } else {
            unsigned long crc;
            /* Gzip header */
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
        }
        break;

    case CGunzipIdx: {
        Tcl_Obj *obj;
        char buf[32768];
        gzFile gin = gzopen(Tcl_GetString(objv[2]), "rb");
        
        if (!gin) {
            Tcl_AppendResult(interp, "nszlib: gunzip: cannot open ", Tcl_GetString(objv[2]), 0);
            result = TCL_ERROR;
        } else {
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
        }
        break;
    }

    case CGzipfileIdx: {
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
	    fclose(fin);
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
    }

    if (outbuf != NULL) {
        ns_free(outbuf);
    }
    return result;
}

static voidpf
ZAlloc(voidpf arg, uInt items, uInt size)
{
    return ns_calloc(items, size);
}

static void
ZFree(voidpf arg, voidpf address)
{
    ns_free(address);
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

unsigned char *Ns_ZlibDeflate(unsigned char *inbuf, unsigned long inlen, unsigned long *outlen)
{
    unsigned char *outbuf;
    z_stream zStream, *z = &zStream;
    int      status;

    memset(z, 0, sizeof(z_stream));
    z->zalloc = ZAlloc;
    z->zfree = ZFree;
    z->opaque = Z_NULL;
    
    status = deflateInit2(z,
                          Z_DEFAULT_COMPRESSION, /* to size memory, will be reset later */
                          Z_DEFLATED, /* method. */
                          -15,        /* windowBits: -8 .. -15 for "raw deflate" */
                          9,          /* memlevel: 1-9 (min-max), default: 8.*/
                          Z_DEFAULT_STRATEGY);
    if (status != Z_OK) {
        Ns_Log(Notice, "Ns_CompressInit: zlib error: %d (%s): %s",
               status, zError(status), (z->msg != NULL) ? z->msg : "(none)");
        return NULL;
    }

    z->next_in = inbuf;
    z->avail_in = inlen;
    
    *outlen = inlen * 1.1 + 20;
    outbuf = ns_malloc(*outlen);

    z->next_out = outbuf;
    z->avail_out = *outlen;

    status = deflate(z, Z_FINISH);

    if (status != Z_OK && status != Z_STREAM_END) {
        Ns_Log(Notice, "Ns_ZlibDeflate: zlib error: %d (%s): %s",
               status, zError(status), (z->msg != NULL) ? z->msg : "(none)");
        ns_free(outbuf);
        outbuf = NULL;
    }
    *outlen = z->total_out;

    status = deflateEnd(z);
    if (status != Z_OK && status != Z_STREAM_END) {
        Ns_Log(Bug, "Ns_ZlibDeflate: deflateEnd: %d (%s): %s",
               status, zError(status), (z->msg != NULL) ? z->msg : "(unknown)");
    }
    return outbuf;
}

unsigned char *Ns_ZlibInflate(unsigned char *inbuf, unsigned long inlen, unsigned long *outlen)
{
    unsigned char *outbuf;
    z_stream zStream, *z = &zStream;
    int      status;

    memset(z, 0, sizeof(z_stream));
    z->zalloc = ZAlloc;
    z->zfree = ZFree;
    z->opaque = Z_NULL;
    
    status = inflateInit2(z, -15 /* windowBits: -8 .. -15 for "raw deflate" */);
    if (status != Z_OK) {
        Ns_Log(Notice, "Ns_ZlibInflate: zlib error: %d (%s): %s",
               status, zError(status), (z->msg != NULL) ? z->msg : "(none)");
        return NULL;
    }

    z->next_in = inbuf;
    z->avail_in = inlen;
    
    *outlen = inlen * 6;
    outbuf = ns_malloc(*outlen);

    z->next_out = outbuf;
    z->avail_out = *outlen;

    while (1) {
        status = inflate(z, Z_NO_FLUSH);
        if (status != Z_OK && status != Z_PARTIAL_FLUSH) {
            Ns_Log(Bug, "Ns_ZlibInflate: inflateBuffer: %d (%s); %s",
                   status, zError(status), (z->msg != NULL) ? z->msg : "(unknown)");
            ns_free(outbuf);
            return NULL;
        }
        if (z->avail_out == 0) {
            long oldSize = *outlen;
            /*
             * double size in case we need more.
             */
            *outlen = *outlen*2;
            outbuf = ns_realloc(outbuf, *outlen);
            z->avail_out = oldSize ;
            z->next_out = outbuf + oldSize;
        } else {
            *outlen = z->total_out;
            break;    
        }
    }

    status = inflateEnd(z);
    if (status != Z_OK && status != Z_STREAM_END) {
        Ns_Log(Bug, "Ns_ZlibInflate: inflateEnd: %d (%s): %s",
               status, zError(status), (z->msg != NULL) ? z->msg : "(unknown)");
    }
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
/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * indent-tabs-mode: nil
 * End:
 */
