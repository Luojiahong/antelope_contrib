#include <stock.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
//#include <stdlib.h>
//#include <unistd.h>

#include "tr.h"



int comp_double ( char * v1, char * v2, void * private )
{
    double d1 = atof( v1 );
    double d2 = atof( v2 );

    if ( d1 == d2 ) return 0;
    if ( d2 > d1 ) return 1;
    return -1;
}

static void help ()
{
    //
    // Just print the help section and exit the code
    //
    printf("\n\n Usage: dbwfserver_extract [-h] [-d] [-b] [-c] [-p page] [-n max_traces] [-m max_points] [-f filter] [-s subset] database time endtime\n");
    printf("\t\t-h                  Print this help and exit.\n");
    printf("\t\t-d                  Run in DEBUG mode. Not valid JSON output!.\n");
    printf("\t\t-c                  Calibrate traces.\n");
    printf("\t\t-b                  Export coverage bars only, no data.\n");
    printf("\t\t-r                  Export in epoch time, not Javascript time.\n");
    printf("\t\t-a                  Include the median value. Builds a 3-tuple for data.\n");
    printf("\t\t-q  'precision'     Set precision on data points on output string.\n");
    printf("\t\t-t  'period'        bin data with this period.\n");
    printf("\t\t-p  'page'          If we have extra traces then show this page.\n");
    printf("\t\t-n  'traces'        Do not export more than this number of traces.\n");
    printf("\t\t-b  'bin_size'      Use this value to bin the data. No binning unless 'max_points' is in use and reached.\n");
    printf("\t\t-m  'max_points'    Hard limit on total return points. Try binning and return less than max_points.\n");
    printf("\t\t-f  'filter'        Use this string to filter the traces. ie 'BW 0.1 4 5.0 4'\n");
    printf("\t\t-s  'regex'         Do a subset on the database using this regex.\n");
    printf("\t\tdatabase            Full path to a database.\n");
    printf("\t\ttime                Start time for extraction.\n");
    printf("\t\tendtime             End time for extraction.\n");
    printf("\n");
    printf("\tANF-UCSD dbwfserver data extraction tool\n");
    printf("\tNO BRTT support!\n");
    printf("\treyes@ucsd.edu\n");
    exit (1);
}

char *
stradd( char * s1, char * s2 )
{

    /* creates a new string by concatenating two strings s1 and s2 together.
    * Unlike strcat, does not alter the original string.  Returns a
    * freshly-allocated char* that must be freed by the caller. */
    char * ns;
    int len;

    if ( !s1 || !s2 ) return NULL;

    len = strlen( s1 ) + strlen( s2 ) + 1;
    ns = (char*) malloc ( len * sizeof(char) );
    sprintf( ns, "%s%s", s1, s2 );

    return strdup(ns);
}


int
main (int argc, char **argv)
{
    int     calibrate=0, errflg=0, maxtr=0, last_page=0, bars=0, maxpoints=0;
    int     templistindex=0, precision=1, javatime=1000, realtime=0, median=0;
    int     c=0, i=0, n=0, page=0, bin=1, bufd=0, debug=0;
    long    nsamp=0,result=0, first_trace=0, last_trace=0, nrecords=0, nrecs=0;
    float   *data=NULL, period=0, *medianval=NULL, *max=NULL, zero=0, *min=NULL;
    float   inf=0, ninf=0;
    double  time=0, endtime=0, samprate=0, start=0, stop=0, total_points=0;
    double  outputperiod=0;
    char    old_sta[16]="", segtype[16]="", sta[16]="", chan[16]="", temp[600]="";
    char    *database=NULL, *dbname=NULL, *subset=NULL, *filter=NULL;
    Dbptr   tr, dbwf, dbsite;
    Tbl     *templist, *fields;
    Hook    *hook=0;

    //
    // Get all command-line options
    //
    while ((c = getopt (argc, argv, "q:t:rabhdcf:s:m:n:p:")) != -1) {
        switch (c) {

        case 'r':
            realtime = 1;
            javatime = 1;
            break;

        case 'a':
            median = 1;
            break;

        case 'd':
            debug = 1;
            break;

        case 'b':
            bars = 1;
            break;

        case 'h':
            help() ;
            break;

        case 'c':
            calibrate = 1;
            break;

        case 'q':
            precision = atoi( optarg );
            break;

        case 't':
            outputperiod = atoi( optarg );
            break;

        case 'f':
            filter =  strdup( optarg );
            break;

        case 's':
            subset = strdup( optarg );
            break;

        case 'm':
            maxpoints = atoi( optarg );
            break;

        case 'n':
            maxtr = atoi( optarg );
            break;

        case 'p':
            page = atoi( optarg );
            break;

        default:
            errflg++;
            if (debug) printf("\nERROR IN FLAG!: [%i]\n",c);
            break;
        }
    }

    if (debug) {
        printf("\nFLAG: period=[%f]\n",outputperiod);
        printf("\nFLAG: precision=[%i]\n",precision);
        printf("\nFLAG: realtime=[%i]\n",realtime);
        printf("\nFLAG: median=[%i]\n",median);
        printf("\nFLAG: debug=[%i]\n",debug);
        printf("\nFLAG: bars=[%i]\n",bars);
        printf("\nFLAG: help()\n");
        printf("\nFLAG: calibrate=[%i]\n",calibrate);
        printf("\nFLAG: filter=[%s]\n",filter);
        printf("\nFLAG: subset=[%s]\n",subset);
        printf("\nFLAG: maxpoints=[%i]\n",maxpoints);
        printf("\nFLAG: maxtr=[%i]\n",maxtr);
        printf("\nFLAG: page=[%i]\n",page);
    }

    if ( errflg ) help();

    //
    // Get Inf and -Inf
    //
    inf = an_infinity();
    ninf = -1 * inf;
    max = &ninf;
    min = &inf;
    if (debug) printf("\nSET: inf=[%f] ninf=[%f]\n",inf,ninf);
    if (debug) printf("\nSET: min=[%f] max=[%f]\n",*min,*max);
    //exit(1);

    if (debug) printf("\nelog_init(%i,%s)\n",argc,*argv);
    elog_init( argc, argv);

    if (debug) printf("\nelog_set(ELOG_DELIVER,0,stdout)\n");
    elog_set( ELOG_DELIVER, 0 ,"stdout");

    //
    // VERIFY FLAGS
    //
    if ( page && ! maxtr ) {
        printf ("{\"ERROR\":\"Need -n MAXTRACES if using -p PAGE flag!\"}\n" ) ;
        exit( 1 );
    }

    //
    // GET COMMAND-LINE ARGS
    //
    if ( argc-optind != 3 ) help();
    database = argv[optind++] ;
    start = atof( argv[optind++] );
    stop = atof( argv[optind++] );
    if (debug) printf("\nARGS: database=[%s]\n",database);
    if (debug) printf("\nARGS: start=[%f]\n",start);
    if (debug) printf("\nARGS: stop=[%f]\n",stop);

    //
    // TEST DBS
    //
    dbname = stradd( database, ".sitechan" );
    if (debug) printf("\nTEST DB [%s]\n",dbname);
    if ( dbopen_table(dbname, "r", &dbsite) == dbINVALID ) {
        printf ("{\"ERROR\":\"Can't open %s\"}\n", dbname ) ;
        exit( 1 );
    }
    dbquery ( dbsite, dbRECORD_COUNT, &nrecords ) ;
    if (debug) printf("\n[%s]dbRECORD_COUNT = [%ld]\n",dbname,nrecords);
    if (! nrecords) {
        printf ("{\"ERROR\":\"EMPTY %s\"}\n", dbname ) ;
        exit( 1 );
    }
    free(dbname);

    dbname = stradd( database, ".wfdisc" );
    if (debug) printf("\nTEST DB [%s]\n",dbname);
    if ( dbopen_table(dbname, "r", &dbwf) == dbINVALID ) {
        printf ("{\"ERROR\":\"Can't open %s\"}\n", dbname ) ;
        exit( 1 );
    }
    dbquery ( dbwf, dbRECORD_COUNT, &nrecords ) ;
    if (debug) printf("\n[%s]dbRECORD_COUNT = [%ld]\n",dbname,nrecords);
    if (! nrecords) {
        printf ("{\"ERROR\":\"EMPTY %s\"}\n", dbname ) ;
        exit( 1 );
    }
    free(dbname);

    //
    // SUBSET SITECHAN TABLE
    //
    if ( subset != NULL )
        sprintf( temp, "%s && ondate <= %s && (offdate >= %s || offdate == -1)", subset,epoch2str(stop,"%Y%j"),epoch2str(start,"%Y%j") );
    else
        sprintf( temp, "ondate <= %s && (offdate >= %s || offdate == -1)", epoch2str(stop,"%Y%j"),epoch2str(start,"%Y%j") );


    if (debug) printf("\ndbsubset(%s)\n",temp);
    dbsite = dbsubset ( dbsite, temp, 0 ) ;

    //
    // UNIQUE SORT SITECHAN TABLE
    //
    fields = strtbl("sta","chan",0) ;
    if (debug) printf("\ndbsort(sta,chan)\n");
    dbsite = dbsort ( dbsite, fields, dbSORT_UNIQUE,"" ) ;
    dbquery ( dbsite, dbRECORD_COUNT, &nrecords ) ;
    if (debug) printf("\ndbRECORD_COUNT = [%ld]\n",nrecords);
    if (! nrecords) {
        printf ("{\"ERROR\":\"No records after subset %s\"}\n", temp ) ;
        exit( 1 );
    }

    //
    // SUBSET WFDISC TABLE
    //
    if ( subset != NULL )
        sprintf( temp, "%s && endtime > %f && time < %f", subset,start,stop );
    else
        sprintf( temp, "endtime > %f && time < %f", start,stop );
    if (debug) printf("\ndbsubset(%s)\n", temp);
    dbwf = dbsubset(dbwf, temp, 0);
    dbquery ( dbwf, dbRECORD_COUNT, &nrecs ) ;
    if (debug) printf("\ndbRECORD_COUNT = [%ld]\n",nrecs);
    if (! nrecs) {
        printf ("{\"ERROR\":\"No records after subset %s\"}\n", temp ) ;
        exit( 1 );
    }

    //
    // VERIFY NUMBER OF TRACES AND PAGES
    //
    first_trace = 0;
    last_trace = nrecords;
    if (debug) printf("\nfirst_trace=[%ld] last_trace=[%ld]\n",first_trace,last_trace);

    if ( bars == 1 ) {
        // ONLY for coverage bars...
        page = 1;
        last_page = 1;
    }
    else {
        // ONLY for wf traces...
        // compare to flags
        if ( ! page  ) page = 1;
        if ( ! maxtr  ) maxtr = nrecords;

        // set last_page
        last_page =  nrecords / maxtr;
        if ( ( nrecords % maxtr ) > 1 ) last_page++;

        // set first and last traces values
        if ( maxtr ) {

            first_trace = ( (page - 1) * maxtr );
            last_trace = first_trace + maxtr ;

            if ( first_trace > nrecords ) {
                printf ("{\"ERROR\":\"No more records. last_page:%i page:%i\"}\n", last_page, page ) ;
                exit( 1 );
            }
        }

    }
    if ( last_trace > nrecords ) last_trace = nrecords;
    if (debug) printf("\nfirst_trace=[%ld] last_trace=[%ld]\n",first_trace,last_trace);

    //
    // START BUILDING JSON OBJECT
    //
    printf ("{\"page\":%i,\"last_page\":%i,",page,last_page) ;
    printf ("\"time\":%0.0f,\"endtime\":%0.0f,",start*javatime,stop*javatime) ;

    if ( filter )
        printf ( "\"filter\":\"%s\",",filter) ;
    else
        printf ( "\"filter\":\"false\",") ;

    if ( calibrate )
        printf ( "\"calib\":\"true\",") ;
    else
        printf ( "\"calib\":\"false\",") ;

    if ( median )
        printf ( "\"median\":\"true\",") ;
    else
        printf ( "\"median\":\"false\",") ;

    if ( realtime )
        printf ( "\"realtime\":\"true\",") ;
    else
        printf ( "\"realtime\":\"false\",") ;

    for ( i = first_trace; i < last_trace; i++ )
    {

        if (debug) printf("\nNext: [%i]\n", i);

        dbsite.record = i;
        dbgetv( dbsite, NULL, "sta", &sta, "chan", &chan, 0 );

        if (debug) printf("\nStart: [%s,%s]\n", sta,chan);

        //
        // JSON SYNTAX
        //
        if ( strcmp(old_sta,sta) != 0 ) {
            if ( strcmp(old_sta,"") != 0 ) {
                // IF THIS IS NOT THE FIRST CLOSE PREV BRACKET
                printf("},");
            }
            // TRACK LAST STATION
            sprintf( old_sta, "%s", sta );
            // NEW STATION AND NEW CHANNEL
            printf("\"%s\":{\"%s\":{", sta,chan);
        }
        else {
            // NEW CHANNEL FOR SAME STATION
            printf(",\"%s\":{", chan);
        }

        if ( bars == 1 ) {
            //
            // COVERAGE BARS
            //

            if (debug) printf("\nCOVERAGE BARS\n");

            printf ( "\"format\":\"bins\"," ) ;
            printf ( "\"type\":\"coverage\"," ) ;
            printf ( "\"data\":[null" ) ;

            // To search from the beginning (including the
            // first record) set the record number < 0
            dbwf.record = -1;

            sprintf( temp, "sta == '%s' && chan == '%s'", sta,chan );

            elog_set( ELOG_DELIVER, 0 ,"/dev/null");

            for ( ;; ) {

                result = dbfind(dbwf, temp, 0, &hook);

                if (debug) printf("\ndbfind[%s] => [%ld]\n",temp,result);

                //printf("\n\t%ld %s \n", result, temp);

                if (result >= 0) {

                    dbwf.record = result ;
                    dbgetv(dbwf,0,"time",&time,"endtime",&endtime,NULL) ;

                    printf( ",[%0.0f,1,%0.0f]", time*javatime , endtime*javatime ) ;

                }
                else { break; }

            }

            printf ( "]" ) ;

            // Clean memory
            free_hook(&hook);

            printf ( "}" ) ;

        }
        else {

            if (debug) printf("\nWAVEFORMS\n");

            // Try to catch any elog msgs from the trloads
            printf ( "\"ERROR\":\"" ) ;

            if (debug) printf("\ntrloadchan[%f,%f,%s,%s]\n",start,stop,sta,chan);

            //
            // Load data into trace object
            //
            tr = dbinvalid() ;
            tr = trloadchan( dbwf, start, stop, sta, chan );

            dbquery ( tr, dbRECORD_COUNT, &nrecs ) ;
            if (debug) printf("\ndbRECORD_COUNT=[%ld]\n",nrecs);
            if (! nrecs) {
                printf ( "\"}" ) ;
                continue;
            }

            // Calibrate trace object if needed
            if ( calibrate ) trapply_calib( tr );
            if (debug) printf("\ntrapply_calib=[%i]\n",calibrate);

            // filter trace object if needed
            if ( filter ) trfilter( tr, filter );
            if (debug) printf("\ntrfilter=[%s]\n",filter);

            tr.record = 0;

            dbgetv(tr,0,"samprate",&samprate,"segtype",&segtype,NULL) ;

            if (debug) printf("\nsamprate=[%f] segtype=[%s]\n",samprate,segtype);

            total_points = samprate * ( stop - start );
            if (debug) printf("\ntotal_points=[%f]\n",total_points);

            // Try to catch any elog msgs from the trloads
            printf ( "\"," ) ;

            period = 1/samprate;

            printf ( "\"type\":\"wf\"," ) ;
            printf ( "\"samprate\":%f,",samprate ) ;
            printf ( "\"segtype\":\"%s\",",segtype ) ;


            templist = newtbl(total_points);

            //
            // Calculate if we need to bin the data
            //
            if ( maxpoints && total_points > maxpoints ) {
                bin = total_points / maxpoints;
                if ( fmod( total_points , maxpoints ) > 1 ) bin++;
                printf ( "\"format\":\"bins\"," ) ;
            }
            else {
                printf ( "\"format\":\"lines\"," ) ;
                bin = 1;
            }

            //
            // Force the size of the bins with the perido flag
            //
            if ( outputperiod ) {
                // bin = samprate * floor( (stop - start)/outputperiod );
                bin = samprate * outputperiod;
                printf ( "\"period\":\"%f\",", outputperiod ) ;
            }

            printf ( "\"data\":[null" ) ;

            if (debug) printf("\nmin=[%f] max=[%f]\n",*min,*max);

            for ( tr.record = 0 ; tr.record < nrecs ; tr.record++ ) {
                if (debug) printf("\ntr.record=[%ld]\n",tr.record);

                dbgetv(tr,0,"time",&time,"nsamp",&nsamp,"data",&data,NULL) ;

                if ( ! nsamp ) continue;

                for ( n=0 ; n<nsamp ; n++ ) {

                    if (debug) printf("\nnext data[%i] => %f [min:%f,max:%f]\n", n, data[n],*min,*max);

                    time += period;

                    if ( isinf(data[n]) ) continue;

                    //
                    // Track elements for median calculation
                    //
                    if (median){
                        //templistindex = maxtbl(templist);
                        //templistindex++;
                        //strputtbl(templist,templistindex,data[n]);
                        pushtbl(templist,&data[n]);
                    }

                    //
                    // Bin data if we need to...
                    //
                    if ( bin > 1 ){

                        // Track max and min of block
                        bufd++;
                        if ( data[n] < *min) min = &data[n];
                        if ( data[n] > *max) max = &data[n];
                        if (debug) printf("\nNow: [min:%f,max:%f]\n",*min,*max);

                    }

                    // Loop if we are not at the end of block
                    if ( bin > 1 && bufd < bin && n < nsamp-1 ) continue;

                    if (debug) printf("\nFinish binnig after %i samples. Write value.\n",bufd);

                    if ( bin > 1 ) {
                        // Test if we have a full bin.
                        if ( bufd >= bin ) {

                            // The time is of the last element in the bin
                            if ( isinf(*min) ) min = &zero;
                            if ( isinf(*max) ) max = &zero;

                            if (median){
                                // Sort and clean the list
                                sorttbl( templist, comp_double, NULL );
                                //templistindex = maxtbl(templist);
                                //trunctbl( templist, templistindex, free );

                                templistindex = floor( maxtbl(templist)/2 );

                                medianval = gettbl(templist,templistindex);

                                printf( ",[%0.0f,%0.*f,%0.*f,%0.*f]", time*javatime , precision, *min, precision, *medianval,precision, *max ) ;
                                clrtbl(templist,0);
                            }
                            else {
                                printf( ",[%0.0f,%0.*f,%0.*f]", time*javatime , precision, *min, precision, *max ) ;
                            }
                        }

                    }
                    else {
                        if ( isinf(data[n]) ) data[n] = 0;
                        printf( ",[%0.0f,%0.*f]", time*javatime , precision, data[n] ) ;
                    }


                    //
                    // Clean temp variables
                    //
                    if (debug) printf("\nReset loop variables: [max:&ninf min:&inf bufd=0].\n");
                    max = &ninf;
                    min = &inf;
                    bufd = 0;
                }

                printf( ",null" );

                if (debug) printf("\nFinish trace: [%ld]\n", tr.record);

            }

            printf ( "]}" ) ;

        }

        if (debug) printf("\nFinish: [%s,%s]\n", sta,chan);
    }
    printf ( "}}\n" ) ;

    return 0;
}

