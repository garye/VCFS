/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#include <memory.h> /* for memset */
#include "cvstool.h"

/* Default timeout can be changed using clnt_control() */
static struct timeval TIMEOUT = { 25, 0 };

cvstool_ls_resp *
cvstool_ls_1(cvstool_ls_args *argp, CLIENT *clnt)
{
	static cvstool_ls_resp clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call (clnt, CVSTOOL_LS,
		(xdrproc_t) xdr_cvstool_ls_args, (caddr_t) argp,
		(xdrproc_t) xdr_cvstool_ls_resp, (caddr_t) &clnt_res,
		TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

cvstool_lsver_resp *
cvstool_lsver_1(cvstool_lsver_args *argp, CLIENT *clnt)
{
	static cvstool_lsver_resp clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call (clnt, CVSTOOL_LSVER,
		(xdrproc_t) xdr_cvstool_lsver_args, (caddr_t) argp,
		(xdrproc_t) xdr_cvstool_lsver_resp, (caddr_t) &clnt_res,
		TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}
