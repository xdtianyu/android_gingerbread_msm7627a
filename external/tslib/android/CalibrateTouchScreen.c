#include "tslib.h"
#include <utils/Log.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/stat.h>

typedef struct {
    int x[5], xfb[5];
    int y[5], yfb[5];
} calibration;

#define ABS_X 0x00
#define ABS_Y 0x01

static struct input_absinfo info_X, info_Y;

int getMaxMinValues(int ts_fd)
{
    int dev_info_fd;
    char dev_info_buffer[50];
    if(ioctl(ts_fd, EVIOCGABS(ABS_X), &info_X)) {
        LOGE("Error reading absolute controller %d for fd %d\n",
             ABS_X, ts_fd);
        return -1;
    }

    if(ioctl(ts_fd, EVIOCGABS(ABS_Y), &info_Y)) {
        LOGE("Error reading absolute controller %d for fd %d\n",
             ABS_Y, ts_fd);
        return -1;
    }

    return 0;
}


int perform_calibration(calibration *cal, int *arr) {
    int j;
    float n, x, y, x2, y2, xy, z, zx, zy;
    float det, a, b, c, e, f, i;
    float scaling = 65536.0;

// Get sums for matrix
    n = x = y = x2 = y2 = xy = 0;
    for(j=0;j<5;j++) {
        n += 1.0;
        x += (float)cal->x[j];
        y += (float)cal->y[j];
        x2 += (float)(cal->x[j]*cal->x[j]);
        y2 += (float)(cal->y[j]*cal->y[j]);
        xy += (float)(cal->x[j]*cal->y[j]);
    }

// Get determinant of matrix -- check if determinant is too small
    det = n*(x2*y2 - xy*xy) + x*(xy*y - x*y2) + y*(x*xy - y*x2);
    if(det < 0.1 && det > -0.1) {
        LOGE("ts_calibrate: determinant is too small -- %f\n",det);
        return 0;
    }

// Get elements of inverse matrix
    a = (x2*y2 - xy*xy)/det;
    b = (xy*y - x*y2)/det;
    c = (x*xy - y*x2)/det;
    e = (n*y2 - y*y)/det;
    f = (x*y - n*xy)/det;
    i = (n*x2 - x*x)/det;

// Get sums for x calibration
    z = zx = zy = 0;
    for(j=0;j<5;j++) {
        z += (float)cal->xfb[j];
        zx += (float)(cal->xfb[j]*cal->x[j]);
        zy += (float)(cal->xfb[j]*cal->y[j]);
    }

// Now multiply out to get the calibration for framebuffer x coord
    arr[2] = (int)((a*z + b*zx + c*zy)*(scaling));
    arr[0] = (int)((b*z + e*zx + f*zy)*(scaling));
    arr[1] = (int)((c*z + f*zx + i*zy)*(scaling));

// Get sums for y calibration
    z = zx = zy = 0;
    for(j=0;j<5;j++) {
        z += (float)cal->yfb[j];
        zx += (float)(cal->yfb[j]*cal->x[j]);
        zy += (float)(cal->yfb[j]*cal->y[j]);
    }

// Now multiply out to get the calibration for framebuffer y coord
    arr[5] = (int)((a*z + b*zx + c*zy)*(scaling));
    arr[3] = (int)((b*z + e*zx + f*zy)*(scaling));
    arr[4] = (int)((c*z + f*zx + i*zy)*(scaling));

// If we got here, we're OK, so assign scaling to a[6] and return
    arr[6] = (int)scaling;
    return 1;

}

/*
    This function is called from init function of linear plugin
    and it calculates the calibration values for linear plugin by
    using values stored in pointercal file.
*/
int calibrateAndroid(int *a, int ts_fd)
{
    //Create the cal file with the coords obtained from java app
    calibration cal;
    char *calfile = NULL;
    int i, j;
    char cal_buffer[256];
    int elements[20];
    char *defaultcalfile = "/data/data/touchscreen.test/files/pointercal";
    int height, width;
    struct stat sbuf;
    int pcal_fd;
    char pcalbuf[200];
    int index;
    char *tokptr;

    if((calfile = getenv("TSLIB_CALIBFILE")) == NULL) calfile = defaultcalfile;
    if(stat(calfile,&sbuf)==0) {
        pcal_fd = open(calfile,O_RDONLY);
        if(pcal_fd < 0){
            LOGE("open failed for pointercal file");
            return -1;
            }
        read(pcal_fd,pcalbuf,200);
        elements[0] = atoi(strtok(pcalbuf," "));
        index=0;
        tokptr = strtok(NULL," ");
        while(tokptr != NULL) {
            index++;
            elements[index] = atoi(tokptr);
            tokptr = strtok(NULL," ");
            }
        close (pcal_fd);

        width = elements[18] * 2;
        height = elements[19] * 2;

        if(getMaxMinValues(ts_fd) < 0){
            LOGE("Cannot access max and min values of touch screen from kernel");
            return -1;
        }

        //Assigning and filling the calibration struct
        j = 0;
        for(i=0; i< ((sizeof(elements))/sizeof(int))/2; i+=2){

            elements[i] = (elements[i] * info_X.maximum)/width;
            elements[i+1] = (elements[i+1] * info_Y.maximum)/height;
            elements[i + 10] = (elements[i + 10] * info_X.maximum)/width;
            elements[i+11] = (elements[i+11] * info_Y.maximum)/height;

            cal.x[j] =  elements[i];
            cal.y[j] =  elements[i+1];
            cal.xfb[j] = elements[i+10];
            cal.yfb[j] = elements[i+11];

            if(0) {
                LOGI("Value of X[%d]=%d in ts_Calibrate\n", j, cal.x[i/2]);
                LOGI("Value of Y[%d]=%d in ts_Calibrate\n", j, cal.y[i/2]);
                LOGI("Value of refX[%d]=%d in ts_Calibrate\n", j, cal.xfb[i/2]);
                LOGI("Value of refY[%d]=%d in ts_Calibrate\n", j, cal.yfb[i/2]);
            }
            j++;
        }

        LOGV("tslib: going to call perform_calibration() \n");
        if (perform_calibration (&cal, a)) {
            LOGV("tslib:perform_calibration succeeded\n ");

            LOGV("Calibration constants: ");
            for (i = 0; i < 7; i++) {
                LOGV("%d ", a[i]);
            }

        }
        else {
            LOGE("Calibration failed.\n");
            i = -1;
        }
    }
    return i;
}
