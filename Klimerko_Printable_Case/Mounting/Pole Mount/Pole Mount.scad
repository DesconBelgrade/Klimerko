/*
    Pole mount for DESCON Klimerko device (https://github.com/DesconBelgrade/Klimerko).
    By Filip Vranesevic.
    INSTRUCTIONS:
    To generate customized mount open this file in OpenSCAD application (www.openscad.org)
    and modify the parameters below. Select "Render" from "Design" menu (or F6) and then 
    "Export->Export as STL..." from "File" menu (or F7) to generate STL file for printing.
*/

/*================================ Prameters to modify ===================================*/

pole = 20;          // Diameter of the pole to attach to 

wall = 3.5;         // Thickness of the mount brace. For small pole diameter use bigger
                    // brace thickness so the mount is big enough to mount the device.
                    
horizontal = false; // Is the pipe horizontal (set true) or vertical (set false)

pole_angle = 45;    // angle by which pole is rotated in relation to the device mount.
                    // Does not have an effect on circular pole.

num_angles = 100;   // The shape of the pole can be any regular polygon. This variable 
                    // sets the number of angles for polygon (eg. 3 for triangle, 4 for  
                    // square, 5 for pentagon...) For circle use some high number like 100.

len = 25;           // The length of the mount on the pole

/*========================================================================================*/


// Internal variables, do not modify unless you know what are you doing
$fn = 100;
dt_w = 18.9;
dt_n = 13.8;
dia = 0.6;
rad = dia/2;
depth = 5;
plate_w = 25;
D2 = pole+(2*wall);
thickness = D2/2;
    
module mount()
{

    width = D2;
    D3 = sqrt(pow(plate_w/2, 2) + pow(D2/2,2))*2;
    offset = (D2-dt_w)/2;

    difference() {
        union() {

            // Main body
            intersection() {
                hull() {
                    translate([rad-wall,0,rad-wall])
                        rotate([90,0,0])
                            cylinder(d=dia, h=len);
                    translate([thickness-rad,0,rad-wall])
                        rotate([90,0,0])
                            cylinder(d=dia, h=len);
                    translate([rad-wall,0,width-rad+wall])
                        rotate([90,0,0])
                            cylinder(d=dia, h=len);
                    translate([thickness-rad,0,width-rad+wall])
                        rotate([90,0,0])
                            cylinder(d=dia, h=len);
                }
                translate([0,0,width/2])
                    rotate([90,0,0])
                        cylinder(d=D3, h=len);
            }

            // Brace
            translate([0,0,width/2])
                rotate([90,pole_angle,0])
                    cylinder(d=D2, h=len, $fn=num_angles);
            
            // Dovetail
            if (horizontal) {
                translate([thickness,-len/2-dt_w/2,width/2-len/2])
                    rotate([-90,0,0])
                        dovetail();
            } else {
                translate([thickness,0,offset])
                    dovetail();
            }
        }
        
        union () {
            
            // Pole hole
            translate([0,0,width/2])
                rotate([90,pole_angle,0])
                    cylinder(d=pole, h=len,$fn=num_angles);
            
            // Gap
            translate([0,-len,-wall])
                cube([0.5, len, D2+(2*wall)]);
            
            // Screw holes
            translate([-D2/2-wall, -len/2, 0])
                rotate([0,90,0])
                    screw();
            translate([-D2/2-wall, -len/2, D2])
                rotate([0,90,0])
                    screw();
        }
    }
}

module screw()
{
    cylinder(d=3.5, h=D2+wall);
    rotate([0,0,30])
        cylinder(d=6.6, h=D2/2, $fn=6);
    if (horizontal) {
        translate([0,0,D2/2+(2*wall)])
            cylinder(d=6.6, h=thickness-wall+depth);
    } else {
        translate([0,0,D2/2+(2*wall)])
            cylinder(d=6.6, h=D2/2-wall);
    }
}


module dovetail()
{
    mid = (dt_w-dt_n)/2;
    difference() {
        translate([rad,0,rad]) {
            hull() {
                translate([depth-dia,0,0])
                    rotate([90,0,0])
                        cylinder(d=dia, h=len);
                translate([depth-dia,0,dt_w-dia])
                    rotate([90,0,0])
                        cylinder(d=dia, h=len);
                translate([0,0,0+mid])
                    rotate([90,0,0])
                        cylinder(d=dia, h=len, $fn=4);
                translate([0,0,dt_n-dia+mid])
                    rotate([90,0,0])
                        cylinder(d=dia, h=len, $fn=4);
            }
        }
        rotate([0,0,-45])
            cube([depth*2, depth*2, dt_w]);
    }   
}


rotate([90,0,0])
    mount();
