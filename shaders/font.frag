#version 450

layout(location=0) in vec4 UV;
layout(location=1) in vec4 Color;

layout(location=0) out vec4 Output;

layout(push_constant) uniform ubo
{
	ivec2 Viewport;	// Window width/height
};

float line(vec2 p, vec2 a, vec2 b)
{
	vec2 pa=p-a, ba=b-a;
    return length(pa-ba*clamp(dot(pa, ba)/dot(ba, ba), 0.0, 1.0));
}

// These functions are re-used by multiple letters
float _u(vec2 uv, float w, float v) {
    return length(vec2(abs(length(vec2(uv.x, max(0.0, -(0.4-v)-uv.y)))-w), max(0.0, uv.y-0.4)));
}

float _i(vec2 uv) {
    return length(vec2(uv.x, max(0.0, abs(uv.y)-0.4)));
}

float _j(vec2 uv) {
    uv+=vec2(0.2, 0.55);
    return uv.x>0.0&&uv.y<0.0?abs(length(uv)-0.25):min(length(uv+vec2(0.0, 0.25)), length(vec2(uv.x-0.25, max(0.0, abs(uv.y-0.475)-0.475))));
}

float _l(vec2 uv) {
    return length(vec2(uv.x, max(0.0, abs(uv.y-0.2)-0.6)));
}

float _o(vec2 uv) {
    return abs(length(vec2(uv.x, max(0.0, abs(uv.y)-0.15)))-0.25);
}

// Lowercase
float aa(vec2 uv) {
    uv=-uv;
    return min(min(abs(length(vec2(max(0.0, abs(uv.x)-0.05), uv.y-0.2))-0.2), length(vec2(uv.x+0.25, max(0.0, abs(uv.y-0.2)-0.2)))), (uv.x<0.0?uv.y<0.0:atan(uv.x, uv.y+0.15)>2.0)?_o(uv):length(vec2(uv.x-0.22734, uv.y+0.254)));
}

float bb(vec2 uv) {
    return min(_o(uv), _l(uv+vec2(0.25, 0.0)));
}

float cc(vec2 uv) {
    return uv.x<0.0||atan(uv.x, abs(uv.y)-0.15)<1.14?_o(uv):min(length(vec2(uv.x+0.25, max(0.0, abs(uv.y)-0.15))), length(vec2(uv.x-0.22734, abs(uv.y)-0.254)));
}

float dd(vec2 uv) {
    return bb(uv*vec2(-1.0, 1.0));
}

float ee(vec2 uv) {
    return min(uv.x<0.0||uv.y>0.05||atan(uv.x, uv.y+0.15)>2.0?_o(uv):length(vec2(uv.x-0.22734, uv.y+0.254)), length(vec2(max(0.0, abs(uv.x)-0.25), uv.y-0.05)));
}

float ff(vec2 uv) {
    return min(_j(vec2(-uv.x+0.05, -uv.y)), length(vec2(max(0.0, abs(uv.x-0.05)-0.25), uv.y-0.4)));
}

float gg(vec2 uv) {
    return min(_o(uv), uv.x>0.0||atan(uv.x, uv.y+0.6)<-2.0?_u(uv, 0.25, -0.2):length(uv+vec2(0.23, 0.7)));
}

float hh(vec2 uv) {
    return min(_u(vec2(uv.x, -uv.y), 0.25, 0.25), _l(vec2(uv.x+0.25, uv.y)));
}

float ii(vec2 uv) {
    return min(_i(uv), length(vec2(uv.x, uv.y-0.6)));
}

float jj(vec2 uv) {
    uv.x+=0.05;
    return min(_j(uv), length(vec2(uv.x-0.05, uv.y-0.6)));
}

float kk(vec2 uv) {
    return min(min(line(uv, vec2(-0.25, -0.1), vec2(0.25, 0.4)), line(uv, vec2(-0.15, 0.0), vec2(0.25, -0.4))), _l(vec2(uv.x+0.25, uv.y)));
}

float ll(vec2 uv) {
    return _l(uv);
}

float mm(vec2 uv) {
    return min(min(_u(vec2(uv.x-0.175, -uv.y), 0.175, 0.175), _u(vec2(uv.x+0.175, -uv.y), 0.175, 0.175)), _i(vec2(uv.x+0.35, -uv.y)));
}

float nn(vec2 uv) {
    return min(_u(vec2(uv.x, -uv.y), 0.25, 0.25), _i(vec2(uv.x+0.25, -uv.y)));
}

float oo(vec2 uv) {
    return _o(uv);
}

float pp(vec2 uv) {
    return min(_o(uv), _l(uv+vec2(0.25, 0.4)));
}

float qq(vec2 uv) {
    return pp(vec2(-uv.x, uv.y));
}

float rr(vec2 uv) {
    uv.x-=0.05;
    return min(atan(uv.x, uv.y-0.15)<1.14&&uv.y>0.0?_o(uv):length(vec2(uv.x-0.22734, uv.y-0.254)), _i(vec2(uv.x+0.25, uv.y)));
}

float ss(vec2 uv) {
    if(uv.y<0.225-uv.x*0.5&&uv.x>0.0||uv.y<-0.225-uv.x*0.5)
        uv=-uv;

    return atan(uv.x-0.05, uv.y-0.2)<1.14?abs(length(vec2(max(0.0, abs(uv.x)-0.05), uv.y-0.2))-0.2):length(vec2(uv.x-0.231505, uv.y-0.284));
}

float tt(vec2 uv) {
	uv=vec2(-uv.x+0.05, uv.y-0.4);
    return min(_j(uv), length(vec2(max(0.0, abs(uv.x-0.05)-0.25), uv.y)));
}

float uu(vec2 uv) {
    return _u(uv, 0.25, 0.25);
}

float vv(vec2 uv) {
    return line(vec2(abs(uv.x), uv.y), vec2(0.25, 0.4), vec2(0.0, -0.4));
}

float ww(vec2 uv) {
    uv.x=abs(uv.x);
	return min(line(uv, vec2(0.3, 0.4), vec2(0.2, -0.4)), line(uv, vec2(0.2, -0.4), vec2(0.0, 0.1)));
}

float xx(vec2 uv) {
    return line(abs(uv), vec2(0.0, 0.0), vec2(0.3, 0.4));
}

float yy(vec2 uv) {
    return min(line(uv, vec2(0.0, -0.2), vec2(-0.3, 0.4)), line(uv, vec2(0.3, 0.4), vec2(-0.3, -0.8)));
}

float zz(vec2 uv) {
    return min(length(vec2(max(0.0, abs(uv.x)-0.25), abs(uv.y)-0.4)), line(uv, vec2(0.25, 0.4), vec2(-0.25, -0.4)));
}

// Uppercase
float AA(vec2 uv) {
    return min(length(vec2(abs(length(vec2(uv.x, max(0.0, uv.y-0.35)))-0.25), min(0.0, uv.y+0.4))), length(vec2(max(0.0, abs(uv.x)-0.25), uv.y-0.1)));
}

float BB(vec2 uv) {
    uv.y=abs(uv.y-0.1);
    return min(length(vec2(abs(length(vec2(max(0.0, uv.x), uv.y-0.25))-0.25), min(0.0, uv.x+0.25))), length(vec2(uv.x+0.25, max(0.0, abs(uv.y)-0.5))));
}

float CC(vec2 uv) {
    float x=abs(length(vec2(uv.x, max(0.0, abs(uv.y-0.1)-0.25)))-0.25);
    uv.y=abs(uv.y-0.1);
    return uv.x<0.0||atan(uv.x, uv.y-0.25)<1.14?x:min(length(vec2(uv.x+0.25, max(0.0, abs(uv.y)-0.25))), length(uv+vec2(-0.22734, -0.354)));
}

float DD(vec2 uv) {
    return min(length(vec2(abs(length(vec2(max(0.0, uv.x), max(0.0, abs(uv.y-0.1)-0.25)))-0.25), min(0.0, uv.x+0.25))), length(vec2(uv.x+0.25, max(0.0, abs(uv.y-0.1)-0.5))));
}

float EE(vec2 uv) {
    uv.y=abs(uv.y-0.1);
    return min(min(length(vec2(max(0.0, abs(uv.x)-0.25), uv.y)), length(vec2(max(0.0, abs(uv.x)-0.25), uv.y-0.5))), length(vec2(uv.x+0.25, max(0.0, abs(uv.y)-0.5))));
}

float FF(vec2 uv) {
    uv.y-=.1;
    return min(min(length(vec2(max(0.0, abs(uv.x)-0.25), uv.y)), length(vec2(max(0.0, abs(uv.x)-0.25), uv.y-0.5))), length(vec2(uv.x+0.25, max(0.0, abs(uv.y)-0.5))));
}

float GG(vec2 uv) {
    uv.y-=0.1;
    float a=atan(uv.x, max(0.0, abs(uv.y)-0.25));
	return min(min(uv.x<0.0||a<1.14||a>3.0?abs(length(vec2(uv.x, max(0.0, abs(uv.y)-0.25)))-0.25):min(length(vec2(uv.x+0.25, max(0.0, abs(uv.y)-0.25))), length(uv+vec2(-0.22734, -0.354))), line(uv, vec2(0.22734, -0.1), vec2(0.22734, -0.354))), line(uv, vec2(0.22734, -0.1), vec2(0.05, -0.1)));
}

float HH(vec2 uv) {
    uv.y-=0.1;
    return min(length(vec2(max(0.0, abs(uv.x)-0.25), uv.y)), length(vec2(abs(uv.x)-0.25, max(0.0, abs(uv.y)-0.5))));
}

float II(vec2 uv) {
    uv.y-=0.1;
    return min(length(vec2(uv.x, max(0.0, abs(uv.y)-0.5))), length(vec2(max(0.0, abs(uv.x)-0.1), abs(uv.y)-0.5)));
}

float JJ(vec2 uv) {
    uv.x+=0.125;
    return min(length(vec2(abs(length(vec2(uv.x, min(0.0, uv.y+0.15)))-0.25), max(0.0, max(-uv.x, uv.y-0.6)))), length(vec2(max(0.0, abs(uv.x-0.125)-0.125), uv.y-0.6)));
}

float KK(vec2 uv) {
    return min(min(line(uv, vec2(-0.25, -0.1), vec2(0.25, 0.6)), line(uv, vec2(-0.1, 0.1), vec2(0.25,-0.4))), length(vec2(uv.x+0.25, max(0.0, abs(uv.y-0.1)-0.5))));
}

float LL(vec2 uv) {
    uv.y-=0.1;
    return min(length(vec2(max(0.0, abs(uv.x)-0.2), uv.y+0.5)), length(vec2(uv.x+0.2, max(0.0, abs(uv.y)-0.5))));
}

float MM(vec2 uv) {
    uv.y-=.1;
    return min(min(min(length(vec2(uv.x-0.35, max(0.0, abs(uv.y)-0.5))), line(uv, vec2(-0.35, 0.5), vec2(0.0, -0.1))), line(uv, vec2(0.0, -0.1), vec2(0.35, 0.5))), length(vec2(uv.x+0.35, max(0.0, abs(uv.y)-0.5))));
}

float NN(vec2 uv) {
    uv.y-=0.1;
    return min(min(length(vec2(uv.x-0.25, max(0.0, abs(uv.y)-0.5))), line(uv, vec2(-0.25, 0.5), vec2(0.25, -0.5))), length(vec2(uv.x+0.25, max(0.0, abs(uv.y)-0.5))));
}

float OO(vec2 uv) {
    return abs(length(vec2(uv.x, max(0.0, abs(uv.y-0.1)-0.25)))-0.25);
}

float PP(vec2 uv) {
    return min(length(vec2(abs(length(vec2(max(0.0, uv.x), uv.y-0.35))-0.25), min(0.0, uv.x+0.25))), length(vec2(uv.x+0.25, max(0.0, abs(uv.y-0.1)-0.5))));
}

float QQ(vec2 uv) {
	return min(abs(length(vec2(uv.x, max(0.0, abs(uv.y-0.1)-0.25)))-0.25), length(vec2(abs((uv.x-0.2)+(uv.y+0.3)), max(0.0, abs((uv.x-0.2)-(uv.y+0.3))-0.2)))/sqrt(2.0));
}

float RR(vec2 uv) {
    return min(min(length(vec2(abs(length(vec2(max(0.0, uv.x), uv.y-0.35))-0.25), min(0.0, uv.x+0.25))), length(vec2(uv.x+0.25, max(0.0, abs(uv.y-0.1)-0.5)))), line(uv, vec2(0.0, 0.1), vec2(0.25,-0.4)));
}

float SS(vec2 uv) {
    uv.y-=0.1;

	if(uv.y<0.275-uv.x*0.5&&uv.x>0.0||uv.y<-0.275-uv.x*0.5)
        uv=-uv;

    return atan(uv.x-0.05, uv.y-0.25)<1.14?abs(length(vec2(max(0.0, abs(uv.x)), uv.y-0.25))-0.25):length(vec2(uv.x-0.236, uv.y-0.332));
}

float TT(vec2 uv) {
	return min(length(vec2(uv.x, max(0., abs(uv.y-0.1)-0.5))), length(vec2(max(0.0, abs(uv.x)-0.25), uv.y-0.6)));
}

float UU(vec2 uv) {
	return length(vec2(abs(length(vec2(uv.x, min(0.0, uv.y+0.15)))-0.25), max(0.0, uv.y-0.6)));
}

float VV(vec2 uv) {
	return line(vec2(abs(uv.x), uv.y), vec2(0.25, 0.6), vec2(0.0, -0.4));
}

float WW(vec2 uv) {
	return min(line(vec2(abs(uv.x), uv.y), vec2(0.3, 0.6), vec2(0.2, -0.4)), line(vec2(abs(uv.x), uv.y), vec2(0.2, -0.4), vec2(0.0, 0.2)));
}

float XX(vec2 uv) {
	return line(abs(vec2(uv.x, uv.y-0.1)), vec2(0.0, 0.0), vec2(0.3, 0.5));
}

float YY(vec2 uv) {
	return min(min(line(uv, vec2(0.0, 0.1), vec2(-0.3, 0.6)), line(uv, vec2(0.0, 0.1), vec2(0.3, 0.6))), length(vec2(uv.x, max(0.0, abs(uv.y+0.15)-0.25))));
}

float ZZ(vec2 uv) {
    return min(length(vec2(max(0.0, abs(uv.x)-0.25), abs(uv.y-0.1)-0.5)), line(uv, vec2(0.25, 0.6), vec2(-0.25, -0.4)));
}

// Numbers
float _11(vec2 uv) {
    return min(min(line(uv, vec2(-0.2, 0.45), vec2(0.0, 0.6)), length(vec2(uv.x, max(0.0, abs(uv.y-0.1)-0.5)))), length(vec2(max(0.0, abs(uv.x)-0.2), uv.y+0.4)));
}

float _22(vec2 uv) {
    float x=min(line(uv, vec2(0.185, 0.17), vec2(-0.25, -0.4)), length(vec2(max(0.0, abs(uv.x)-0.25), uv.y+0.4)));
    uv+=vec2(0.025, -0.35);
    return min(x, abs(atan(uv.x, uv.y)-0.63)<1.64?abs(length(uv)-0.275):length(uv+vec2(0.23, -0.15)));
}

float _33(vec2 uv) {
    uv.y=abs(uv.y-0.1)-0.25;
    return atan(uv.x, uv.y)>-1.0?abs(length(uv)-0.25):min(length(uv+vec2(0.211, -0.134)), length(uv+vec2(0.0, 0.25)));
}

float _44(vec2 uv) {
    return min(min(length(vec2(uv.x-0.15, max(0.0, abs(uv.y-0.1)-0.5))), line(uv, vec2(0.15, 0.6), vec2(-0.25, -0.1))), length(vec2(max(0.0, abs(uv.x)-0.25), uv.y+0.1)));
}

float _55(vec2 uv) {
	return min(min(length(vec2(max(0.0, abs(uv.x)-0.25), uv.y-0.6)), length(vec2(uv.x+0.25, max(0.0, abs(uv.y-0.36)-0.236)))), abs(atan(uv.x+0.1, uv.y+0.05)+1.57)<0.86&&uv.x+0.05<0.0?length(uv+vec2(0.3, 0.274)):abs(length(vec2(uv.x+0.05, max(0.0, abs(uv.y+0.1)-0.0)))-0.3));
}

float _66(vec2 uv) {
    uv.y-=0.075;
    uv=-uv;
    float b=abs(length(vec2(uv.x, max(0.0, abs(uv.y)-0.275)))-0.25);
    uv.y-=0.175;
    return min(abs(length(vec2(uv.x, max(0.0, abs(uv.y)-0.05)))-0.25), cos(atan(uv.x, uv.y+0.45)+0.65)<0.0||(uv.x>0.0&&uv.y<0.0)?b:length(uv+vec2(0.2, 0.6)));
}

float _77(vec2 uv) {
    return min(length(vec2(max(0.0, abs(uv.x)-0.25), uv.y-0.6)), line(uv, vec2(-0.25, -0.39), vec2(0.25, 0.6)));
}

float _88(vec2 uv) {
    return min(abs(length(vec2(uv.x, abs(uv.y-0.1)-0.245))-0.255), length(vec2(max(0.0, abs(uv.x)-0.08), uv.y-0.1+uv.x*0.07)));
}

float _99(vec2 uv) {
    return min(abs(length(vec2(uv.x, max(0.0, abs(uv.y-0.3)-0.05)))-0.25), cos(atan(uv.x, uv.y+0.15)+0.65)<0.0||(uv.x>0.0&&uv.y<0.3)?abs(length(vec2(uv.x, max(0.0, abs(uv.y-0.125)-0.275)))-0.25):length(uv+vec2(0.2, 0.3)));
}

float _00(vec2 uv) {
    return abs(length(vec2(uv.x, max(0.0, abs(uv.y-0.1)-0.25)))-0.25);
}

// Special
float period(vec2 uv) {
	return length(vec2(uv.x, uv.y+0.4))*0.97;
}

float exclamation(vec2 uv) {
	return min(period(uv), length(vec2(uv.x, max(0., abs(uv.y-.2)-.4)))-uv.y*.06);
}

float quote(vec2 uv) {
	return min(line(uv, vec2(-0.15, 0.6), vec2(-0.15, 0.8)), line(uv, vec2(0.15, 0.6), vec2(0.15, 0.8)));
}

float pound(vec2 uv) {
	uv.y-=0.1;
	uv.x-=uv.y*0.1;
	uv=abs(uv);
	return min(length(vec2(uv.x-0.125, max(0.0, abs(uv.y)-0.3))), length(vec2(max(0.0, abs(uv.x)-0.25), uv.y-0.125)));
}

float dollersign(vec2 uv) {
	return min(ss(uv), length(vec2(uv.x, max(0.0, abs(uv.y)-0.5))));
}

float percent(vec2 uv) {
	return min(min(abs(length(uv+vec2(-0.2, 0.2))-0.1)-0.0001, abs(length(uv+vec2(0.2, -0.2))-0.1)-0.0001), line(uv, vec2(-0.2, -0.4), vec2(0.2, 0.4)));
}

float ampersand(vec2 uv) {
	uv+=vec2(0.05, -0.44);
	float x=min(min(abs(atan(uv.x, uv.y))<2.356?abs(length(uv)-0.15):1.0, line(uv, vec2(-0.106, -0.106), vec2(0.4, -0.712))), line(uv, vec2(0.106, -0.106), vec2(-0.116, -0.397)));
	uv+=vec2(-0.025, 0.54);
	return min(min(x, abs(atan(uv.x, uv.y)-0.785)>1.57?abs(length(uv)-0.2):1.0), line(uv, vec2(0.141, -0.141), vec2(0.377, 0.177)));
}

float apostrophe(vec2 uv) {
	return line(uv, vec2(0.0, 0.6), vec2(0.0, 0.8));
}

float leftparenthesis(vec2 uv) {
	return abs(atan(uv.x-0.62, uv.y)+1.57)<1.0?abs(length(uv-vec2(0.62, 0.0))-0.8):length(vec2(uv.x-0.185, abs(uv.y)-0.672));
}

float rightparenthesis(vec2 uv) {
	return leftparenthesis(-uv);
}

float asterisk(vec2 uv) {
	uv=abs(vec2(uv.x, uv.y-0.1));
	return min(line(uv, vec2(0.866*0.25, 0.5*0.25), vec2(0.0)), length(vec2(uv.x, max(0.0, abs(uv.y)-0.25))));
}

float plus(vec2 uv) {
	return min(length(vec2(max(0.0, abs(uv.x)-0.25), uv.y-0.1)), length(vec2(uv.x, max(0.0, abs(uv.y-0.1)-0.25))));
}

float comma(vec2 uv) {
	return min(period(uv), line(uv, vec2(.031, -.405), vec2(-.029, -.52)));
}

float minus(vec2 uv) {
	return length(vec2(max(0.0, abs(uv.x)-0.25), uv.y-0.1));
}

float forwardslash(vec2 uv) {
	return line(uv, vec2(-0.25, -0.4), vec2(0.25, 0.6));
}

float colon(vec2 uv) {
	return length(vec2(uv.x, abs(uv.y-0.1)-0.25));
}

float semicolon(vec2 uv) {
	return min(length(vec2(uv.x, abs(uv.y-0.1)-0.25)), line(uv-vec2(0.0, 0.1), vec2(0.0, -0.28), vec2(-0.029, -0.32)));
}

float less(vec2 uv) {
	return line(vec2(uv.x, abs(uv.y-0.1)), vec2(0.25, 0.25), vec2(-0.25, 0.0));
}

float _equal(vec2 uv) {
	return length(vec2(max(0.0, abs(uv.x)-0.25), abs(uv.y-0.1)-0.15));
}

float greater(vec2 uv) {
	return less(vec2(-uv.x, uv.y));
}

float question(vec2 uv) {
	return min(min(period(uv), length(vec2(uv.x, max(0.0, abs(uv.y+0.035)-0.1125)))), abs(atan(uv.x+0.025, uv.y-0.35)-1.05)<2.?abs(length(uv+vec2(0.025, -0.35))-.275):length(uv+vec2(0.25, -0.51))-.0);
}

float at(vec2 uv) {
	// TODO: This needs clean-up, clearly I'm no SDF artist
	uv=mat2(0.5, -0.8660254, 0.8660254, 0.5)*uv;
	vec2 sc=vec2(0.34202, -0.93969);
	float outside=(sc.y*abs(uv.x)>sc.x*uv.y)?length(vec2(abs(uv.x), uv.y)-0.55*sc)-0.0001:abs(length(vec2(abs(uv.x), uv.y))-0.55)-0.0001;
	uv=mat2(-0.139173, -0.990268, 0.990268, -0.139173)*uv+vec2(0.35, 0.1);
	sc=vec2(1.0, 0.0);
	float inside=(sc.y*abs(uv.x)>sc.x*uv.y)?length(vec2(abs(uv.x), uv.y)-0.155*sc)-0.0001:abs(length(vec2(abs(uv.x), uv.y))-0.155)-0.0001;
	return min(min(outside, abs(length(uv-vec2(0.35, 0.1))-0.2)-0.0001), inside);
}

float leftsquare(vec2 uv) {
	return min(length(vec2(uv.x+0.125, max(0.0, abs(uv.y-0.1)-0.5))), length(vec2(max(0.0, abs(uv.x)-0.125), abs(uv.y-0.1)-0.5)));
}

float backslash(vec2 uv) {
	return forwardslash(vec2(-uv.x, uv.y));
}

float rightsquare(vec2 uv) {
	return leftsquare(vec2(-uv.x, uv.y));
}

float circumflex(vec2 uv) {
	return less(-vec2(uv.y*1.5-0.7, uv.x*1.5-0.2));
}

float underline(vec2 uv) {
	return length(vec2(max(0.0, abs(uv.x)-0.25), uv.y+0.4));
}

float grave(vec2 uv) {
	return line(uv, vec2(0.0, 0.6), vec2(-0.1, 0.8));
}

float leftcurly(vec2 uv) {
	return length(vec2(abs(length(vec2((uv.x*sign(abs(uv.y-0.1)-0.25)-0.2), max(0.0, abs(abs(uv.y-0.1)-0.25)-0.05)))-0.2), max(0.0, abs(uv.x)-0.2)));

}

float pipe(vec2 uv) {
	return length(vec2(uv.x, max(0.0, abs(uv.y-0.1)-0.5)));
}

float rightcurly(vec2 uv) {
	return leftcurly(vec2(-uv.x, uv.y));
}

float tilde(vec2 uv) {
	float s=0.8191529, c=0.5735751;
	vec2 one=mat2(-c, s, -s, -c)*(uv+vec2(-0.118, -0.3));
	vec2 two=mat2(c, -s, s, c)*(uv+vec2(0.118, 0.025));

	return min(length(vec2(length(max(one, vec2(0.0)))-0.2+min(max(one.x, one.y), 0.0), min(min(one.x, one.y), 0.0))), length(vec2(length(max(two, vec2(0.0)))-0.2+min(max(two.x, two.y), 0.0), min(min(two.x, two.y), 0.0))));
}

float sdfDistanceW(float dist, float w)
{
	return smoothstep(-w, w, -dist);
}

void main()
{
	const float onePixel=1.0/UV.w;
	vec3 dist=vec3(1.0);

	switch(uint(UV.z))
	{
		case 33:	dist.x=exclamation(UV.xy-vec2(onePixel.x, 0.0));		dist.y=exclamation(UV.xy);		dist.z=exclamation(UV.xy+vec2(onePixel.x, 0.0));		break;
		case 34:	dist.x=quote(UV.xy-vec2(onePixel.x, 0.0));				dist.y=quote(UV.xy);			dist.z=quote(UV.xy+vec2(onePixel.x, 0.0));				break;
		case 35:	dist.x=pound(UV.xy-vec2(onePixel.x, 0.0));				dist.y=pound(UV.xy);			dist.z=pound(UV.xy+vec2(onePixel.x, 0.0));				break;
		case 36:	dist.x=dollersign(UV.xy-vec2(onePixel.x, 0.0));			dist.y=dollersign(UV.xy);		dist.z=dollersign(UV.xy+vec2(onePixel.x, 0.0));			break;
		case 37:	dist.x=percent(UV.xy-vec2(onePixel.x, 0.0));			dist.y=percent(UV.xy);			dist.z=percent(UV.xy+vec2(onePixel.x, 0.0));			break;
		case 38:	dist.x=ampersand(UV.xy-vec2(onePixel.x, 0.0));			dist.y=ampersand(UV.xy);		dist.z=ampersand(UV.xy+vec2(onePixel.x, 0.0));			break;
		case 39:	dist.x=apostrophe(UV.xy-vec2(onePixel.x, 0.0));			dist.y=apostrophe(UV.xy);		dist.z=apostrophe(UV.xy+vec2(onePixel.x, 0.0));			break;
		case 40:	dist.x=leftparenthesis(UV.xy-vec2(onePixel.x, 0.0));	dist.y=leftparenthesis(UV.xy);	dist.z=leftparenthesis(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 41:	dist.x=rightparenthesis(UV.xy-vec2(onePixel.x, 0.0));	dist.y=rightparenthesis(UV.xy);	dist.z=rightparenthesis(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 42:	dist.x=asterisk(UV.xy-vec2(onePixel.x, 0.0));			dist.y=asterisk(UV.xy);			dist.z=asterisk(UV.xy+vec2(onePixel.x, 0.0));			break;
		case 43:	dist.x=plus(UV.xy-vec2(onePixel.x, 0.0));				dist.y=plus(UV.xy);				dist.z=plus(UV.xy+vec2(onePixel.x, 0.0));				break;
		case 44:	dist.x=comma(UV.xy-vec2(onePixel.x, 0.0));				dist.y=comma(UV.xy);			dist.z=comma(UV.xy+vec2(onePixel.x, 0.0));				break;
		case 45:	dist.x=minus(UV.xy-vec2(onePixel.x, 0.0));				dist.y=minus(UV.xy);			dist.z=minus(UV.xy+vec2(onePixel.x, 0.0));				break;
		case 46:	dist.x=period(UV.xy-vec2(onePixel.x, 0.0));				dist.y=period(UV.xy);			dist.z=period(UV.xy+vec2(onePixel.x, 0.0));				break;
		case 47:	dist.x=forwardslash(UV.xy-vec2(onePixel.x, 0.0));		dist.y=forwardslash(UV.xy);		dist.z=forwardslash(UV.xy+vec2(onePixel.x, 0.0));		break;

		case 48:    dist.x=_00(UV.xy-vec2(onePixel.x, 0.0));	dist.y=_00(UV.xy);	dist.z=_00(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 49:    dist.x=_11(UV.xy-vec2(onePixel.x, 0.0));	dist.y=_11(UV.xy);	dist.z=_11(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 50:    dist.x=_22(UV.xy-vec2(onePixel.x, 0.0));	dist.y=_22(UV.xy);	dist.z=_22(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 51:    dist.x=_33(UV.xy-vec2(onePixel.x, 0.0));	dist.y=_33(UV.xy);	dist.z=_33(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 52:    dist.x=_44(UV.xy-vec2(onePixel.x, 0.0));	dist.y=_44(UV.xy);	dist.z=_44(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 53:    dist.x=_55(UV.xy-vec2(onePixel.x, 0.0));	dist.y=_55(UV.xy);	dist.z=_55(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 54:    dist.x=_66(UV.xy-vec2(onePixel.x, 0.0));	dist.y=_66(UV.xy);	dist.z=_66(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 55:    dist.x=_77(UV.xy-vec2(onePixel.x, 0.0));	dist.y=_77(UV.xy);	dist.z=_77(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 56:    dist.x=_88(UV.xy-vec2(onePixel.x, 0.0));	dist.y=_88(UV.xy);	dist.z=_88(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 57:    dist.x=_99(UV.xy-vec2(onePixel.x, 0.0));	dist.y=_99(UV.xy);	dist.z=_99(UV.xy+vec2(onePixel.x, 0.0));	break;

		case 58:	dist.x=colon(UV.xy-vec2(onePixel.x, 0.0));		dist.y=colon(UV.xy);		dist.z=colon(UV.xy+vec2(onePixel.x, 0.0));		break;
		case 59:	dist.x=semicolon(UV.xy-vec2(onePixel.x, 0.0));	dist.y=semicolon(UV.xy);	dist.z=semicolon(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 60:	dist.x=less(UV.xy-vec2(onePixel.x, 0.0));		dist.y=less(UV.xy);			dist.z=less(UV.xy+vec2(onePixel.x, 0.0));		break;
		case 61:	dist.x=_equal(UV.xy-vec2(onePixel.x, 0.0));		dist.y=_equal(UV.xy);		dist.z=_equal(UV.xy+vec2(onePixel.x, 0.0));		break;
		case 62:	dist.x=greater(UV.xy-vec2(onePixel.x, 0.0));	dist.y=greater(UV.xy);		dist.z=greater(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 63:	dist.x=question(UV.xy-vec2(onePixel.x, 0.0));	dist.y=question(UV.xy);		dist.z=question(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 64:	dist.x=at(UV.xy-vec2(onePixel.x, 0.0));			dist.y=at(UV.xy);			dist.z=at(UV.xy+vec2(onePixel.x, 0.0));			break;

		case 65: 	dist.x=AA(UV.xy-vec2(onePixel.x, 0.0));	dist.y=AA(UV.xy);	dist.z=AA(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 66: 	dist.x=BB(UV.xy-vec2(onePixel.x, 0.0));	dist.y=BB(UV.xy);	dist.z=BB(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 67: 	dist.x=CC(UV.xy-vec2(onePixel.x, 0.0));	dist.y=CC(UV.xy);	dist.z=CC(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 68: 	dist.x=DD(UV.xy-vec2(onePixel.x, 0.0));	dist.y=DD(UV.xy);	dist.z=DD(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 69: 	dist.x=EE(UV.xy-vec2(onePixel.x, 0.0));	dist.y=EE(UV.xy);	dist.z=EE(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 70: 	dist.x=FF(UV.xy-vec2(onePixel.x, 0.0));	dist.y=FF(UV.xy);	dist.z=FF(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 71: 	dist.x=GG(UV.xy-vec2(onePixel.x, 0.0));	dist.y=GG(UV.xy);	dist.z=GG(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 72: 	dist.x=HH(UV.xy-vec2(onePixel.x, 0.0));	dist.y=HH(UV.xy);	dist.z=HH(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 73: 	dist.x=II(UV.xy-vec2(onePixel.x, 0.0));	dist.y=II(UV.xy);	dist.z=II(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 74: 	dist.x=JJ(UV.xy-vec2(onePixel.x, 0.0));	dist.y=JJ(UV.xy);	dist.z=JJ(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 75: 	dist.x=KK(UV.xy-vec2(onePixel.x, 0.0));	dist.y=KK(UV.xy);	dist.z=KK(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 76: 	dist.x=LL(UV.xy-vec2(onePixel.x, 0.0));	dist.y=LL(UV.xy);	dist.z=LL(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 77: 	dist.x=MM(UV.xy-vec2(onePixel.x, 0.0));	dist.y=MM(UV.xy);	dist.z=MM(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 78: 	dist.x=NN(UV.xy-vec2(onePixel.x, 0.0));	dist.y=NN(UV.xy);	dist.z=NN(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 79: 	dist.x=OO(UV.xy-vec2(onePixel.x, 0.0));	dist.y=OO(UV.xy);	dist.z=OO(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 80: 	dist.x=PP(UV.xy-vec2(onePixel.x, 0.0));	dist.y=PP(UV.xy);	dist.z=PP(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 81: 	dist.x=QQ(UV.xy-vec2(onePixel.x, 0.0));	dist.y=QQ(UV.xy);	dist.z=QQ(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 82: 	dist.x=RR(UV.xy-vec2(onePixel.x, 0.0));	dist.y=RR(UV.xy);	dist.z=RR(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 83: 	dist.x=SS(UV.xy-vec2(onePixel.x, 0.0));	dist.y=SS(UV.xy);	dist.z=SS(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 84: 	dist.x=TT(UV.xy-vec2(onePixel.x, 0.0));	dist.y=TT(UV.xy);	dist.z=TT(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 85: 	dist.x=UU(UV.xy-vec2(onePixel.x, 0.0));	dist.y=UU(UV.xy);	dist.z=UU(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 86: 	dist.x=VV(UV.xy-vec2(onePixel.x, 0.0));	dist.y=VV(UV.xy);	dist.z=VV(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 87: 	dist.x=WW(UV.xy-vec2(onePixel.x, 0.0));	dist.y=WW(UV.xy);	dist.z=WW(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 88: 	dist.x=XX(UV.xy-vec2(onePixel.x, 0.0));	dist.y=XX(UV.xy);	dist.z=XX(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 89: 	dist.x=YY(UV.xy-vec2(onePixel.x, 0.0));	dist.y=YY(UV.xy);	dist.z=YY(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 90: 	dist.x=ZZ(UV.xy-vec2(onePixel.x, 0.0));	dist.y=ZZ(UV.xy);	dist.z=ZZ(UV.xy+vec2(onePixel.x, 0.0));	break;

		case 91:	dist.x=leftsquare(UV.xy-vec2(onePixel.x, 0.0));		dist.y=leftsquare(UV.xy);	dist.z=leftsquare(UV.xy+vec2(onePixel.x, 0.0));		break;
		case 92:	dist.x=backslash(UV.xy-vec2(onePixel.x, 0.0));		dist.y=backslash(UV.xy);	dist.z=backslash(UV.xy+vec2(onePixel.x, 0.0));		break;
		case 93:	dist.x=rightsquare(UV.xy-vec2(onePixel.x, 0.0));	dist.y=rightsquare(UV.xy);	dist.z=rightsquare(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 94:	dist.x=circumflex(UV.xy-vec2(onePixel.x, 0.0));		dist.y=circumflex(UV.xy);	dist.z=circumflex(UV.xy+vec2(onePixel.x, 0.0));		break;
		case 95:	dist.x=underline(UV.xy-vec2(onePixel.x, 0.0));		dist.y=underline(UV.xy);	dist.z=underline(UV.xy+vec2(onePixel.x, 0.0));		break;
		case 96:	dist.x=grave(UV.xy-vec2(onePixel.x, 0.0));			dist.y=grave(UV.xy);		dist.z=grave(UV.xy+vec2(onePixel.x, 0.0));			break;

		case 97: 	dist.x=aa(UV.xy-vec2(onePixel.x, 0.0));	dist.y=aa(UV.xy);	dist.z=aa(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 98: 	dist.x=bb(UV.xy-vec2(onePixel.x, 0.0));	dist.y=bb(UV.xy);	dist.z=bb(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 99: 	dist.x=cc(UV.xy-vec2(onePixel.x, 0.0));	dist.y=cc(UV.xy);	dist.z=cc(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 100:	dist.x=dd(UV.xy-vec2(onePixel.x, 0.0));	dist.y=dd(UV.xy);	dist.z=dd(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 101:	dist.x=ee(UV.xy-vec2(onePixel.x, 0.0));	dist.y=ee(UV.xy);	dist.z=ee(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 102:	dist.x=ff(UV.xy-vec2(onePixel.x, 0.0));	dist.y=ff(UV.xy);	dist.z=ff(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 103:	dist.x=gg(UV.xy-vec2(onePixel.x, 0.0));	dist.y=gg(UV.xy);	dist.z=gg(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 104:	dist.x=hh(UV.xy-vec2(onePixel.x, 0.0));	dist.y=hh(UV.xy);	dist.z=hh(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 105:	dist.x=ii(UV.xy-vec2(onePixel.x, 0.0));	dist.y=ii(UV.xy);	dist.z=ii(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 106:	dist.x=jj(UV.xy-vec2(onePixel.x, 0.0));	dist.y=jj(UV.xy);	dist.z=jj(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 107:	dist.x=kk(UV.xy-vec2(onePixel.x, 0.0));	dist.y=kk(UV.xy);	dist.z=kk(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 108:	dist.x=ll(UV.xy-vec2(onePixel.x, 0.0));	dist.y=ll(UV.xy);	dist.z=ll(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 109:	dist.x=mm(UV.xy-vec2(onePixel.x, 0.0));	dist.y=mm(UV.xy);	dist.z=mm(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 110:	dist.x=nn(UV.xy-vec2(onePixel.x, 0.0));	dist.y=nn(UV.xy);	dist.z=nn(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 111:	dist.x=oo(UV.xy-vec2(onePixel.x, 0.0));	dist.y=oo(UV.xy);	dist.z=oo(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 112:	dist.x=pp(UV.xy-vec2(onePixel.x, 0.0));	dist.y=pp(UV.xy);	dist.z=pp(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 113:	dist.x=qq(UV.xy-vec2(onePixel.x, 0.0));	dist.y=qq(UV.xy);	dist.z=qq(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 114:	dist.x=rr(UV.xy-vec2(onePixel.x, 0.0));	dist.y=rr(UV.xy);	dist.z=rr(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 115:	dist.x=ss(UV.xy-vec2(onePixel.x, 0.0));	dist.y=ss(UV.xy);	dist.z=ss(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 116:	dist.x=tt(UV.xy-vec2(onePixel.x, 0.0));	dist.y=tt(UV.xy);	dist.z=tt(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 117:	dist.x=uu(UV.xy-vec2(onePixel.x, 0.0));	dist.y=uu(UV.xy);	dist.z=uu(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 118:	dist.x=vv(UV.xy-vec2(onePixel.x, 0.0));	dist.y=vv(UV.xy);	dist.z=vv(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 119:	dist.x=ww(UV.xy-vec2(onePixel.x, 0.0));	dist.y=ww(UV.xy);	dist.z=ww(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 120:	dist.x=xx(UV.xy-vec2(onePixel.x, 0.0));	dist.y=xx(UV.xy);	dist.z=xx(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 121:	dist.x=yy(UV.xy-vec2(onePixel.x, 0.0));	dist.y=yy(UV.xy);	dist.z=yy(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 122:	dist.x=zz(UV.xy-vec2(onePixel.x, 0.0));	dist.y=zz(UV.xy);	dist.z=zz(UV.xy+vec2(onePixel.x, 0.0));	break;

		case 123:	dist.x=leftcurly(UV.xy-vec2(onePixel.x, 0.0)); 	dist.y=leftcurly(UV.xy);	dist.z=leftcurly(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 124:	dist.x=pipe(UV.xy-vec2(onePixel.x, 0.0));		dist.y=pipe(UV.xy);			dist.z=pipe(UV.xy+vec2(onePixel.x, 0.0));		break;
		case 125:	dist.x=rightcurly(UV.xy-vec2(onePixel.x, 0.0));	dist.y=rightcurly(UV.xy);	dist.z=rightcurly(UV.xy+vec2(onePixel.x, 0.0));	break;
		case 126:	dist.x=tilde(UV.xy-vec2(onePixel.x, 0.0));		dist.y=tilde(UV.xy);		dist.z=tilde(UV.xy+vec2(onePixel.x, 0.0));		break;
	};

	vec3 subPix=vec3(sdfDistanceW(dist.x-0.065, onePixel.x), sdfDistanceW(dist.y-0.065, onePixel.x), sdfDistanceW(dist.z-0.065, onePixel.x));
	subPix=pow(subPix, vec3(1.0/2.2));
	Output=vec4(Color.xyz*subPix, (subPix.x+subPix.y+subPix.z)/3.0);
}
