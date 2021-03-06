/* 
 * Antelope Toolbox for Matlab
 *
 * Kent Lindquist
 * Geophysical Institute
 * University of Alaska, Fairbanks
 * 1997
 */

#define USAGE "Error using ==> dbget_remark\n\n\
Usage: REMARK = DBGET_REMARK ( DBPTR )\n"

#include "antelope_mex.h"

void mexFunction ( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	Dbptr	db;
	char	*remark;
	int	result;

	if( nrhs != 1 )
	{
		antelope_mexUsageMsgTxt ( USAGE );
		return;
	}
        else if( ! get_dbptr( prhs[0], &db ) )
        {
                antelope_mexUsageMsgTxt ( USAGE );
		return;
        }

	result = dbget_remark( db, &remark );
	antelope_mex_clear_register( 1 );
	if( result < 0 ) mexErrMsgTxt( "dbget_remark failed\n" );

	plhs[0] = mxCreateString( remark );
	free( remark );
}
