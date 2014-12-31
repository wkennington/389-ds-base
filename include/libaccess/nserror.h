/** BEGIN COPYRIGHT BLOCK
 * This Program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; version 2 of the License.
 * 
 * This Program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this Program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307 USA.
 * 
 * In addition, as a special exception, Red Hat, Inc. gives You the additional
 * right to link the code of this Program with code not covered under the GNU
 * General Public License ("Non-GPL Code") and to distribute linked combinations
 * including the two, subject to the limitations in this paragraph. Non-GPL Code
 * permitted under this exception must only link to the code of this Program
 * through those well defined interfaces identified in the file named EXCEPTION
 * found in the source code files (the "Approved Interfaces"). The files of
 * Non-GPL Code may instantiate templates or use macros or inline functions from
 * the Approved Interfaces without causing the resulting work to be covered by
 * the GNU General Public License. Only Red Hat, Inc. may make changes or
 * additions to the list of Approved Interfaces. You must obey the GNU General
 * Public License in all respects for all of the Program code and other code used
 * in conjunction with the Program except the Non-GPL Code covered by this
 * exception. If you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you do not wish to
 * provide this exception without modification, you must delete this exception
 * statement from your version and license this file solely under the GPL without
 * exception. 
 * 
 * 
 * Copyright (C) 2001 Sun Microsystems, Inc. Used by permission.
 * Copyright (C) 2005 Red Hat, Inc.
 * All rights reserved.
 * END COPYRIGHT BLOCK **/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef __nserror_h
#define __nserror_h

#ifndef NOINTNSACL
#define INTNSACL
#endif /* !NOINTNSACL */

/*
 * Description (nserror.h)
 *
 *	This file describes the interface to an error handling mechanism
 *	that is intended for general use.  This mechanism uses a data
 *	structure known as an "error frame" to capture information about
 *	an error.  Multiple error frames are used in nested function calls
 *	to capture the interpretation of an error at the different levels
 *	of a nested call.
 */

#include <stdarg.h>
#include <prtypes.h>
#include "public/nsacl/nserrdef.h"

#ifdef INTNSACL

NSPR_BEGIN_EXTERN_C

/* Functions in nseframe.c */
extern void nserrDispose(NSErr_t * errp);
extern NSEFrame_t * nserrFAlloc(NSErr_t * errp);
extern void nserrFFree(NSErr_t * errp, NSEFrame_t * efp);
extern NSEFrame_t * nserrGenerate(NSErr_t * errp, long retcode, long errorid,
				  const char * program, int errc, ...);

/* Functions in nserrmsg.c */
extern char * nserrMessage(NSEFrame_t * efp, int flags);
extern char * nserrRetrieve(NSEFrame_t * efp, int flags);

NSPR_END_EXTERN_C

#endif /* INTNSACL */

#endif /* __nserror_h */