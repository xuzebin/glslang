#version 300 es
precision highp float;
uniform int c, d;
in highp float x;

void main()
{
    float f;
    int a[2];

    switch(f) {      // ERROR
    }

    switch(a) {      // ERROR
    }

    switch(c)
    {
    }

    switch(c)        // ERROR, not enough stuff after last label
    {
    case 2:
    }

    switch(c)
    {
        f = sin(x);   // ERRROR
    case 2:
        f = cos(x);
        break;
    }

    switch (c) {
    default:
        break;
    case 1:
        f = sin(x);
        break;
    case 2:
        f = cos(x);
        break;
    default:           // ERROR, 2nd default
        f = tan(x);
    }

    switch (c) {
    case 1:
        f = sin(x);
        break;
    case 2:
        switch (d) {
        case 1:
            f = x * x * x;
            break;
        case 2:
            f = x * x;
            break;
        }
        break;
    default:
        f = tan(x);
    case 1:           // ERROR, 2nd 'case 1'
        break;
    case 3.8:         // ERROR, non-int
        break;
    case c:           // ERROR, non-constant
        break;       
    }

    switch (c) {      // a no-error normal switch
    case 1:
        f = sin(x);
        break;
    case 2:
        switch (d) {
        case 1:
            f = x * x * x;
            break;
        case 2:
            f = x * x;
            break;
        }
        break;
    default:
        f = tan(x);
    }

    break;            // ERROR
}
