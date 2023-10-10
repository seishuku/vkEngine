// SDF
#version 450

layout (location=0) in vec3 UV;
layout (location=1) in vec4 Color;

layout (binding=0) uniform sampler2DArray Texture;

layout (location=0) out vec4 Output;

layout (push_constant) uniform ubo {
	ivec2 Viewport;	// Window width/height
};

#define pi 3.141
#define deg(x) (x*pi/180.0-pi)

// line function, used in k, s, v, w, x, y, z
float line(vec2 p, vec2 a, vec2 b)
{
	vec2 pa = p - a;
	vec2 ba = b - a;
	float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

//These functions are re-used by multiple letters
float _u(vec2 uv,float w,float v) {
    return length(vec2(
                abs(length(vec2(uv.x,
                                max(0.0,-(.4-v)-uv.y) ))-w)
               ,max(0.,uv.y-.4))) +.4;
}
float _i(vec2 uv) {
    return length(vec2(uv.x,max(0.,abs(uv.y)-.4)))+.4;
}
float _j(vec2 uv) {
    uv.x+=.2;
    float t = _u(uv,.25,-.15);
    float x = uv.x>0.?t:length(vec2(uv.x,uv.y+.8))+.4;
    return x;
}
float _l(vec2 uv) {
    uv.y -= .2;
    return length(vec2(uv.x,max(0.,abs(uv.y)-.6)))+.4;
}
float _o(vec2 uv) {
    return abs(length(vec2(uv.x,max(0.,abs(uv.y)-.15)))-.25)+.4;
}

float aa(vec2 uvo) {
  // uv.y = abs(uv.y);
  float yOfs = 0.4;
  float center = yOfs * 0.5;
  float a = atan(uvo.x*sign(uvo.y),abs(uvo.y)-center);
  
  vec2 uv = vec2(uvo.x,a>deg(0.0) && a<deg(90.0) ? -abs(uvo.y):abs(uvo.y));
  float x = abs(length(
        vec2( max(0., abs(uv.x) - 0.25 + center),
              uv.y - 0.5 * yOfs))
                 -yOfs * 0.5 );
  x = min(x,length(vec2(uvo.x+0.25,uvo.y-center)));
  // x = a>deg(45.0) && a<deg(115.0) ? length(vec2(x,abs(a-deg(115.0))*0.25)) : x;
  return min(x,length(vec2(uvo.x-0.25,max(0.0,abs(uvo.y+0.25*yOfs)-0.75*yOfs))));
}

float bb(vec2 uv) {
    float x = _o(uv);
    uv.x += .25;
    return min(x,_l(uv));
}
float cc(vec2 uv) {
    float x = _o(uv);
    uv.y= abs(uv.y);
    return uv.x<0.||atan(uv.x,uv.y-0.15)<1.14?x:
                    min(length(vec2(uv.x+.25,max(0.0,abs(uv.y)-.15))),//makes df right 
                        length(uv+vec2(-.22734,-.254)));
}
float dd(vec2 uv) {
    uv.x *= -1.;
    return bb(uv);
}
float ee(vec2 uv) {
    float x = _o(uv);
    return min(uv.x<0.||uv.y>.05||atan(uv.x,uv.y+0.15)>2.?x:length(vec2(uv.x-.22734,uv.y+.254)),
               length(vec2(max(0.,abs(uv.x)-.25),uv.y-.05)));
}
float ff(vec2 uv) {
    uv.x *= -1.;
    uv.x += .05;
    float x = _j(vec2(uv.x,-uv.y));
    uv.y -= .4;
    x = min(x,length(vec2(max(0.,abs(uv.x-.05)-.25),uv.y)));
    return x;
}
float gg(vec2 uv) {
    float x = _o(uv);
    return min(x,uv.x>0.||atan(uv.x,uv.y+.6)<-2.?
               _u(uv,0.25,-0.2):
               length(uv+vec2(.23,.7)));
}
float hh(vec2 uv) {
    uv.y *= -1.;
    float x = _u(uv,.25,.25);
    uv.x += .25;
    uv.y *= -1.;
    return min(x,_l(uv));
}
float ii(vec2 uv) {
    return min(_i(uv),length(vec2(uv.x,uv.y-.6)));
}
float jj(vec2 uv) {
    uv.x+=.05;
    return min(_j(uv),length(vec2(uv.x-.05,uv.y-.6)));
}
float kk(vec2 uv) {
    float x = line(uv,vec2(-.25,-.1), vec2(0.25,0.4));
    x = min(x,line(uv,vec2(-.15,.0), vec2(0.25,-0.4)));
    uv.x+=.25;
    return min(x,_l(uv));
}
float ll(vec2 uv) {
    return _l(uv);
}
float mm(vec2 uv) {
    //uv.x *= 1.4;
    uv.y *= -1.;
    uv.x-=.175;
    float x = _u(uv,.175,.175);
    uv.x+=.35;
    x = min(x,_u(uv,.175,.175));
    uv.x+=.175;
    return min(x,_i(uv));
}
float nn(vec2 uv) {
    uv.y *= -1.;
    float x = _u(uv,.25,.25);
    uv.x+=.25;
    return min(x,_i(uv));
}
float oo(vec2 uv) {
    return _o(uv);
}
float pp(vec2 uv) {
    float x = _o(uv);
    uv.x += .25;
    uv.y += .4;
    return min(x,_l(uv));
}
float qq(vec2 uv) {
    uv.x = -uv.x;
    return pp(uv);
}
float rr(vec2 uv) {
    uv.x -= .05;
    float x =atan(uv.x,uv.y-0.15)<1.14&&uv.y>0.?_o(uv):length(vec2(uv.x-.22734,uv.y-.254));
    
    //)?_o(uv):length(vec2(uv.x-.22734,uv.y+.254))+.4);
    
    uv.x+=.25;
    return min(x,_i(uv));
}
float ss_old(vec2 uv) {
    if (uv.y <.225-uv.x*.5 && uv.x>0. || uv.y<-.225-uv.x*.5)
        uv = -uv;
    float a = abs(length(vec2(max(0.,abs(uv.x)-.05),uv.y-.2))-.2);
    float b = length(vec2(uv.x-.231505,uv.y-.284));
    float x = atan(uv.x-.05,uv.y-0.2)<1.14?a:b;
    return x;
}
float ss(vec2 uvo) {
  float yOfs = 0.4;
  float center = yOfs * 0.5;
  float a = atan(uvo.x*sign(uvo.y),abs(uvo.y)-center);
  
  vec2 uv = vec2(uvo.x, a>deg(270.0) && a<deg(360.0) ? -abs(uvo.y):abs(uvo.y));
  float x = abs(length(
        vec2( max(0., abs(uv.x) - 0.25 + center),
              uv.y - center))
                 -center );
  x = min(x,length(vec2(uvo.x+0.25,uvo.y-center)));
  return x;
}

float tt(vec2 uv) {
    uv.x *= -1.;
    uv.y -= .4;
    uv.x += .05;
    float x = min(_j(uv),length(vec2(max(0.,abs(uv.x-.05)-.25),uv.y)));
    return x;
}
float uu(vec2 uv) {
    return _u(uv,.25,.25);
}
float vv(vec2 uv) {
    uv.x=abs(uv.x);
    return line(uv,vec2(0.25,0.4), vec2(0.,-0.4));
}
float ww(vec2 uv) {
    uv.x=abs(uv.x);
    return min(line(uv,vec2(0.3,0.4), vec2(.2,-0.4)),
               line(uv,vec2(0.2,-0.4), vec2(0.,0.1)));
}
float xx(vec2 uv) {
    uv=abs(uv);
    return line(uv,vec2(0.,0.), vec2(.3,0.4));
}
float yy(vec2 uv) {
    return min(line(uv,vec2(.0,-.2), vec2(-.3,0.4)),
               line(uv,vec2(.3,.4), vec2(-.3,-0.8)));
}
float zz(vec2 uv) {
    float l = line(uv,vec2(0.25,0.4), vec2(-0.25,-0.4));
    uv.y=abs(uv.y);
    float x = length(vec2(max(0.,abs(uv.x)-.25),uv.y-.4));
    return min(x,l);
}

// Capitals
float AA(vec2 uv) {
    float x = length(vec2(
                abs(length(vec2(uv.x,
                                max(0.0,uv.y-.35) ))-0.25)
               ,min(0.,uv.y+.4)));
    return min(x,length(vec2(max(0.,abs(uv.x)-.25),uv.y-.1) ));
}

float BB(vec2 uv) {
    uv.y -=.1;
    uv.y = abs(uv.y);
    float x = length(vec2(
                abs(length(vec2(max(0.0,uv.x),
                                 uv.y-.25))-0.25)
               ,min(0.,uv.x+.25)));
    return min(x,length(vec2(uv.x+.25,max(0.,abs(uv.y)-.5)) ));
}
float CC(vec2 uv) {
    float x = abs(length(vec2(uv.x,max(0.,abs(uv.y-.1)-.25)))-.25);
    uv.y -= .1;
    uv.y= abs(uv.y);
    return uv.x<0.||atan(uv.x,uv.y-0.25)<1.14?x:
                    min(length(vec2(uv.x+.25,max(0.0,abs(uv.y)-.25))),//makes df right 
                        length(uv+vec2(-.22734,-.354)));
}
float DD(vec2 uv) {
    uv.y -=.1;
    //uv.y = abs(uv.y);
    float x = length(vec2(
                abs(length(vec2(max(0.0,uv.x),
                                max(0.0,abs(uv.y)-.25)))-0.25)
               ,min(0.,uv.x+.25)));
    return min(x,length(vec2(uv.x+.25,max(0.,abs(uv.y)-.5)) ));
}
float EE(vec2 uv) {
    uv.y -=.1;
    uv.y = abs(uv.y);
    float x = min(length(vec2(max(0.,abs(uv.x)-.25),uv.y)),
                  length(vec2(max(0.,abs(uv.x)-.25),uv.y-.5)));
    return min(x,length(vec2(uv.x+.25,max(0.,abs(uv.y)-.5))));
}
float FF(vec2 uv) {
    uv.y -=.1;
    float x = min(length(vec2(max(0.,abs(uv.x)-.25),uv.y)),
                  length(vec2(max(0.,abs(uv.x)-.25),uv.y-.5)));
    return min(x,length(vec2(uv.x+.25,max(0.,abs(uv.y)-.5))));
}
float GG(vec2 uv) {
    float x = abs(length(vec2(uv.x,max(0.,abs(uv.y-.1)-.25)))-.25);
    uv.y -= .1;
    float a = atan(uv.x,max(0.,abs(uv.y)-0.25));
    x = uv.x<0.||a<1.14 || a>3.?x:
                    min(length(vec2(uv.x+.25,max(0.0,abs(uv.y)-.25))),//makes df right 
                        length(uv+vec2(-.22734,-.354)));
    x = min(x,line(uv,vec2(.22734,-.1),vec2(.22734,-.354)));
    return min(x,line(uv,vec2(.22734,-.1),vec2(.05,-.1)));
}
float HH(vec2 uv) {
    uv.y -=.1;
    uv.x = abs(uv.x);
    float x = length(vec2(max(0.,abs(uv.x)-.25),uv.y));
    return min(x,length(vec2(uv.x-.25,max(0.,abs(uv.y)-.5))));
}
float II(vec2 uv) {
    uv.y -= .1;
    float x = length(vec2(uv.x,max(0.,abs(uv.y)-.5)));
    uv.y = abs(uv.y);
    return min(x,length(vec2(max(0.,abs(uv.x)-.1),uv.y-.5)));
}
float JJ(vec2 uv) {
    uv.x += .125;
    float x = length(vec2(
                abs(length(vec2(uv.x,
                                min(0.0,uv.y+.15) ))-0.25)
               ,max(0.,max(-uv.x,uv.y-.6))));
    return min(x,length(vec2(max(0.,abs(uv.x-.125)-.125),uv.y-.6)));
}
float KK(vec2 uv) {
    float x = line(uv,vec2(-.25,-.1), vec2(0.25,0.6));
    x = min(x,line(uv,vec2(-.1, .1), vec2(0.25,-0.4)));
//    uv.x+=.25;
    return min(x,length(vec2(uv.x+.25,max(0.,abs(uv.y-.1)-.5))));
}
float LL(vec2 uv) {
    uv.y -=.1;
    float x = length(vec2(max(0.,abs(uv.x)-.2),uv.y+.5));
    return min(x,length(vec2(uv.x+.2,max(0.,abs(uv.y)-.5))));
}
float MM(vec2 uv) {
    uv.y-=.1;
    float x = min(length(vec2(uv.x-.35,max(0.,abs(uv.y)-.5))),
                  line(uv,vec2(-.35,.5),vec2(.0,-.1)));
    x = min(x,line(uv,vec2(.0,-.1),vec2(.35,.5)));
    return min(x,length(vec2(uv.x+.35,max(0.,abs(uv.y)-.5))));
}
float NN(vec2 uv) {
    uv.y-=.1;
    float x = min(length(vec2(uv.x-.25,max(0.,abs(uv.y)-.5))),
                  line(uv,vec2(-.25,.5),vec2(.25,-.5)));
    return min(x,length(vec2(uv.x+.25,max(0.,abs(uv.y)-.5))));
}
float OO(vec2 uv) {
    return abs(length(vec2(uv.x,max(0.,abs(uv.y-.1)-.25)))-.25);
}
float PP(vec2 uv) {
    float x = length(vec2(
                abs(length(vec2(max(0.0,uv.x),
                                 uv.y-.35))-0.25)
               ,min(0.,uv.x+.25)));
    return min(x,length(vec2(uv.x+.25,max(0.,abs(uv.y-.1)-.5)) ));
}
float QQ(vec2 uv) {
    float x = abs(length(vec2(uv.x,max(0.,abs(uv.y-.1)-.25)))-.25);
    uv.y += .3;
    uv.x -= .2;
    return min(x,length(vec2(abs(uv.x+uv.y),max(0.,abs(uv.x-uv.y)-.2)))/sqrt(2.));
}
float RR(vec2 uv) {
    float x = length(vec2(
                abs(length(vec2(max(0.0,uv.x),
                                 uv.y-.35))-0.25)
               ,min(0.,uv.x+.25)));
    x = min(x,length(vec2(uv.x+.25,max(0.,abs(uv.y-.1)-.5)) ));
    return min(x,line(uv,vec2(0.0,0.1),vec2(0.25,-0.4)));
}
float SS(vec2 uv) {
    uv.y -= .1;
    if (uv.y <.275-uv.x*.5 && uv.x>0. || uv.y<-.275-uv.x*.5)
        uv = -uv;
    float a = abs(length(vec2(max(0.,abs(uv.x)),uv.y-.25))-.25);
    float b = length(vec2(uv.x-.236,uv.y-.332));
    float x = atan(uv.x-.05,uv.y-0.25)<1.14?a:b;
    return x;
}
float TT(vec2 uv) {
    uv.y -= .1;
    float x = length(vec2(uv.x,max(0.,abs(uv.y)-.5)));
    return min(x,length(vec2(max(0.,abs(uv.x)-.25),uv.y-.5)));
}
float UU(vec2 uv) {
    float x = length(vec2(
                abs(length(vec2(uv.x,
                                min(0.0,uv.y+.15) ))-0.25)
               ,max(0.,uv.y-.6)));
    return x;
}
float VV(vec2 uv) {
    uv.x=abs(uv.x);
    return line(uv,vec2(0.25,0.6), vec2(0.,-0.4));
}
float WW(vec2 uv) {
    uv.x=abs(uv.x);
    return min(line(uv,vec2(0.3,0.6), vec2(.2,-0.4)),
               line(uv,vec2(0.2,-0.4), vec2(0.,0.2)));
}
float XX(vec2 uv) {
    uv.y -= .1;
    uv=abs(uv);
    return line(uv,vec2(0.,0.), vec2(.3,0.5));
}
float YY(vec2 uv) {
    return min(min(line(uv,vec2(.0, .1), vec2(-.3, 0.6)),
                   line(uv,vec2(.0, .1), vec2( .3, 0.6))),
                   length(vec2(uv.x,max(0.,abs(uv.y+.15)-.25))));
}
float ZZ(vec2 uv) {
    float l = line(uv,vec2(0.25,0.6), vec2(-0.25,-0.4));
    uv.y-=.1;
    uv.y=abs(uv.y);
    float x = length(vec2(max(0.,abs(uv.x)-.25),uv.y-.5));
    return min(x,l);
}

//Numbers
float _11(vec2 uv) {
    return min(min(
             line(uv,vec2(-0.2,0.45),vec2(0.,0.6)),
             length(vec2(uv.x,max(0.,abs(uv.y-.1)-.5)))),
             length(vec2(max(0.,abs(uv.x)-.2),uv.y+.4)));
             
}
float _22(vec2 uv) {
    float x = min(line(uv,vec2(0.185,0.17),vec2(-.25,-.4)),
                  length(vec2(max(0.,abs(uv.x)-.25),uv.y+.4)));
    uv.y-=.35;
    uv.x += 0.025;
    return min(x,abs(atan(uv.x,uv.y)-0.63)<1.64?abs(length(uv)-.275):
               length(uv+vec2(.23,-.15)));
}
float _33(vec2 uv) {
    uv.y-=.1;
    uv.y = abs(uv.y);
    uv.y-=.25;
    return atan(uv.x,uv.y)>-1.?abs(length(uv)-.25):
           min(length(uv+vec2(.211,-.134)),length(uv+vec2(.0,.25)));
}
float _44(vec2 uv) {
    float x = min(length(vec2(uv.x-.15,max(0.,abs(uv.y-.1)-.5))),
                  line(uv,vec2(0.15,0.6),vec2(-.25,-.1)));
    return min(x,length(vec2(max(0.,abs(uv.x)-.25),uv.y+.1)));
}
float _55(vec2 uv) {
    float b = min(length(vec2(max(0.,abs(uv.x)-.25),uv.y-.6)),
                  length(vec2(uv.x+.25,max(0.,abs(uv.y-.36)-.236))));
    uv.y += 0.1;
    uv.x += 0.05;
    float c = abs(length(vec2(uv.x,max(0.,abs(uv.y)-.0)))-.3);
    return min(b,abs(atan(uv.x,uv.y)+1.57)<.86 && uv.x<0.?
               length(uv+vec2(.2,.224))
               :c);
}
float _66(vec2 uv) {
    uv.y-=.075;
    uv = -uv;
    float b = abs(length(vec2(uv.x,max(0.,abs(uv.y)-.275)))-.25);
    uv.y-=.175;
    float c = abs(length(vec2(uv.x,max(0.,abs(uv.y)-.05)))-.25);
    return min(c,cos(atan(uv.x,uv.y+.45)+0.65)<0.||(uv.x>0.&& uv.y<0.)?b:
               length(uv+vec2(0.2,0.6)));
}
float _77(vec2 uv) {
    return min(length(vec2(max(0.,abs(uv.x)-.25),uv.y-.6)),
               line(uv,vec2(-0.25,-0.39),vec2(0.25,0.6)));
}
float _88(vec2 uv) {
    float l = length(vec2(max(0.,abs(uv.x)-.08),uv.y-.1+uv.x*.07));
    uv.y-=.1;
    uv.y = abs(uv.y);
    uv.y-=.245;
    return min(abs(length(uv)-.255),l);
}
float _99(vec2 uv) {
    uv.y-=.125;
    float b = abs(length(vec2(uv.x,max(0.,abs(uv.y)-.275)))-.25);
    uv.y-=.175;
    float c = abs(length(vec2(uv.x,max(0.,abs(uv.y)-.05)))-.25);
    return min(c,cos(atan(uv.x,uv.y+.45)+0.65)<0.||(uv.x>0.&& uv.y<0.)?b:
               length(uv+vec2(0.2,0.6)));
}
float _00(vec2 uv) {
    uv.y-=.1;
    return abs(length(vec2(uv.x,max(0.,abs(uv.y)-.25)))-.25);
}

float sdfDistance(float dist)
{
//    float px = 1.0/Viewport.x;
//    float weight = 0.12;
//    return smoothstep(weight-px,weight+px, dist);
    float px=fwidth(dist);
    return smoothstep(0.49-px,0.49+px, dist);
}

void main()
{
	float dist=0.0;
    uint ch=uint(UV.z);

    switch(ch)
    {
        case 65:    dist=AA(UV.xy); break;
        case 66:    dist=BB(UV.xy); break;
        case 67:    dist=CC(UV.xy); break;
        case 68:    dist=DD(UV.xy); break;
        case 69:    dist=EE(UV.xy); break;
        case 70:    dist=FF(UV.xy); break;
        case 71:    dist=GG(UV.xy); break;
        case 72:    dist=HH(UV.xy); break;
        case 73:    dist=II(UV.xy); break;
        case 74:    dist=JJ(UV.xy); break;
        case 75:    dist=KK(UV.xy); break;
        case 76:    dist=LL(UV.xy); break;
        case 77:    dist=MM(UV.xy); break;
        case 78:    dist=NN(UV.xy); break;
        case 79:    dist=OO(UV.xy); break;
        case 80:    dist=PP(UV.xy); break;
        case 81:    dist=QQ(UV.xy); break;
        case 82:    dist=RR(UV.xy); break;
        case 83:    dist=SS(UV.xy); break;
        case 84:    dist=TT(UV.xy); break;
        case 85:    dist=UU(UV.xy); break;
        case 86:    dist=VV(UV.xy); break;
        case 87:    dist=WW(UV.xy); break;
        case 88:    dist=XX(UV.xy); break;
        case 89:    dist=YY(UV.xy); break;
        case 90:    dist=ZZ(UV.xy); break;

        case 97:    dist=aa(UV.xy); break;
        case 98:    dist=bb(UV.xy); break;
        case 99:    dist=cc(UV.xy); break;
        case 100:   dist=dd(UV.xy); break;
        case 101:   dist=ee(UV.xy); break;
        case 102:   dist=ff(UV.xy); break;
        case 103:   dist=gg(UV.xy); break;
        case 104:   dist=hh(UV.xy); break;
        case 105:   dist=ii(UV.xy); break;
        case 106:   dist=jj(UV.xy); break;
        case 107:   dist=kk(UV.xy); break;
        case 108:   dist=ll(UV.xy); break;
        case 109:   dist=mm(UV.xy); break;
        case 110:   dist=nn(UV.xy); break;
        case 111:   dist=oo(UV.xy); break;
        case 112:   dist=pp(UV.xy); break;
        case 113:   dist=qq(UV.xy); break;
        case 114:   dist=rr(UV.xy); break;
        case 115:   dist=ss(UV.xy); break;
        case 116:   dist=tt(UV.xy); break;
        case 117:   dist=uu(UV.xy); break;
        case 118:   dist=vv(UV.xy); break;
        case 119:   dist=ww(UV.xy); break;
        case 120:   dist=xx(UV.xy); break;
        case 121:   dist=yy(UV.xy); break;
        case 122:   dist=zz(UV.xy); break;
    };

    float alpha=sdfDistance(texture(Texture, UV).x);//sdfDistance(dist);

	Output=vec4(Color.xyz, alpha);
}
