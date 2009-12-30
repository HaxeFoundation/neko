/* ************************************************************************ */
/*																			*/
/*  Tora - Neko Application Server											*/
/*  Copyright (c)2008 Motion-Twin											*/
/*																			*/
/* This library is free software; you can redistribute it and/or			*/
/* modify it under the terms of the GNU Lesser General Public				*/
/* License as published by the Free Software Foundation; either				*/
/* version 2.1 of the License, or (at your option) any later version.		*/
/*																			*/
/* This library is distributed in the hope that it will be useful,			*/
/* but WITHOUT ANY WARRANTY; without even the implied warranty of			*/
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU		*/
/* Lesser General Public License or the LICENSE file for more details.		*/
/*																			*/
/* ************************************************************************ */
package tora;

enum Code {
	CFile;
	CUri;
	CClientIP;
	CGetParams;
	CPostData;
	CHeaderKey;
	CHeaderValue;
	CHeaderAddValue;
	CParamKey;
	CParamValue;
	CHostName;
	CHttpMethod;
	CExecute;
	CError;
	CPrint;
	CLog;
	CFlush;
	CRedirect;
	CReturnCode;
	CQueryMultipart;
	CPartFilename;
	CPartKey;
	CPartData;
	CPartDone;
	CTestConnect;
	CListen;
	CHostResolve;
}
