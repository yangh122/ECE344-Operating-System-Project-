#include <assert.h>
#include "common.h"
#include "point.h"
#include <math.h>

void point_translate(struct point *p, double x, double y)
{
    point_set(p, x+point_X(p), y + point_Y(p));
}

double point_distance(const struct point *p1, const struct point *p2)
{
    
    double x_dis = (p1->x)-(p2->x);
    double y_dis = (p1->y)-(p2->y);  
    double ans = sqrt(x_dis*x_dis + y_dis*y_dis);  
    return ans;
}

int point_compare(const struct point *p1, const struct point *p2){   
    
    struct point O_point;
    
    point_set(&O_point, 0.0, 0.0);
    
    double p1_dis = point_distance(p1,&O_point);
    double p2_dis = point_distance(p2,&O_point);
    
    if(p1_dis > p2_dis){
        return 1;
    }
    else if(p1_dis==p2_dis){
        return 0;
    }
        return -1;


}
