display('Running dbexample_cggrid_ny')

if( ~ cggrid_supported )
        display('There is no cggrid support in your copy of Antelope')
        return
end

echo on

[X,Y] = meshgrid(-2:0.2:2,-3:0.3:3);
Z = exp( -X.^2 - Y.^2 );
cgg = cggrid( X, Y, Z );

ny = cggrid_ny( cgg )

clear( cgg );

echo off
