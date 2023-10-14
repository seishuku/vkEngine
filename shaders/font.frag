#version 450

layout(location=0) in vec3 UV;
layout(location=1) in vec4 Color;

layout(location=0) out vec4 Output;

layout(push_constant) uniform ubo
{
	ivec2 Viewport;	// Window width/height
};

// Common functions used by multiple letters
#define PI 3.1415926

float line(vec2 p, vec2 a, vec2 b)
{
	vec2 pa=p-a;
	vec2 ba=b-a;
	return length(pa-ba*clamp(dot(pa, ba)/dot(ba, ba), 0.0, 1.0));
}

float _u(vec2 uv, float w, float v)
{
	return length(vec2(abs(length(vec2(uv.x, max(0.0, -(0.4-v)-uv.y)))-w), max(0.0, uv.y-0.4)));
}

float _i(vec2 uv)
{
	return length(vec2(uv.x, max(0.0, abs(uv.y)-0.4)));
}

float _j(vec2 uv)
{
	uv+=vec2(0.2, 0.55);
	return uv.x>0.0&&uv.y<0.0?abs(length(uv)-0.25):min(length(uv+vec2(0.0, 0.25)), length(vec2(uv.x-0.25, max(0.0, abs(uv.y-0.475)-0.475))));
}

float _l(vec2 uv)
{
	return length(vec2(uv.x, max(0.0, abs(uv.y-0.2)-0.6)));
}

float _o(vec2 uv)
{
	return abs(length(vec2(uv.x, max(0.0, abs(uv.y)-0.15)))-0.25);
}

// Lowercase
float aa(vec2 uvo)
{
	float a=atan(uvo.x*sign(uvo.y), abs(uvo.y)-0.2);
	vec2 uv=vec2(uvo.x, a>-PI&&a<-PI*0.5?-abs(uvo.y):abs(uvo.y));
	return min(min(abs(length(vec2(max(0.0, abs(uv.x)-0.05), uv.y-0.2))-0.2), length(vec2(uvo.x+0.25, uvo.y-0.2))), length(vec2(uvo.x-0.25, max(0.0, abs(uvo.y+0.1)-0.3))));
}

float bb(vec2 uv)
{
	return min(_o(uv), _l(uv+vec2(0.25, 0.0)));
}

float cc(vec2 uv)
{
	return uv.x<0.0||atan(uv.x, abs(uv.y)-0.15)<1.14?_o(uv):min(length(vec2(uv.x+0.25, max(0.0, abs(uv.y)-0.15))), length(uv+vec2(-0.22734, -0.254)));
}

float dd(vec2 uv)
{
	return bb(uv*vec2(-1.0, 1.0));
}

float ee(vec2 uv)
{
	return min(uv.x<0.0||uv.y>0.05||atan(uv.x, uv.y+0.15)>2.0?_o(uv):length(vec2(uv.x-0.22734, uv.y+0.254)), length(vec2(max(0.0, abs(uv.x)-0.25), uv.y-0.05)));
}

float ff(vec2 uv)
{
	return min(_j(vec2(-uv.x+0.05, -uv.y)), length(vec2(max(0.0, abs(-uv.x)-0.25), uv.y-0.4)));
}

float gg(vec2 uv)
{
	return min(_o(uv), uv.x>0.0||atan(uv.x, uv.y+0.6)<-2.0?_u(uv, 0.25, -0.2):length(uv+vec2(0.23, 0.7)));
}

float hh(vec2 uv)
{
	return min(_u(uv*vec2(1.0, -1.0), 0.25, 0.25), _l(uv+vec2(0.25, 0.0)));
}

float ii(vec2 uv)
{
	return min(_i(uv), length(vec2(uv.x, uv.y-0.6)));
}

float jj(vec2 uv)
{
	return min(_j(uv+vec2(0.05, 0.0)), length(vec2(uv.x, uv.y-0.6)));
}

float kk(vec2 uv)
{
	return min(min(line(uv, vec2(-0.25, -0.1), vec2(0.25, 0.4)), line(uv, vec2(-0.15, 0.0), vec2(0.25, -0.4))), _l(uv+vec2(0.25, 0.0)));
}

float ll(vec2 uv)
{
	return _l(uv);
}

float mm(vec2 uv)
{
	return min(min(_u(-uv-vec2(0.175, 0.0), 0.175, 0.175), _u(-uv+vec2(0.175, 0.0), 0.175, 0.175)), _i(uv+vec2(0.35, 0.0)));
}

float nn(vec2 uv)
{
	return min(_u(-uv, 0.25, 0.25), _i(uv+vec2(0.25, 0.0)));
}

float oo(vec2 uv)
{
	return _o(uv);
}

float pp(vec2 uv)
{
	return min(_o(uv), _l(uv+vec2(0.25, 0.4)));
}

float qq(vec2 uv)
{
	return pp(vec2(-uv.x, uv.y));
}

float rr(vec2 uv)
{
	return min(atan(uv.x-0.05, uv.y-0.15)<1.14&&uv.y>0.?_o(uv-vec2(0.05, 0.0)):length(vec2(uv.x-0.27734, uv.y-0.254)), _i(uv+vec2(0.2, 0.0)));
}

float ss(vec2 uvo)
{
	float a=atan(uvo.x*sign(uvo.y), abs(uvo.y)-0.2);
	vec2 uv=vec2(uvo.x, a>PI*0.5&&a<PI?-abs(uvo.y):abs(uvo.y));
	return min(abs(length(vec2(max(0.0, abs(uv.x)-0.05), uv.y-0.2))-0.2), length(vec2(uvo.x+0.25, uvo.y-0.2)));
}

float tt(vec2 uv)
{
	return min(_j(vec2(-uv.x+0.05, uv.y-0.4)), length(vec2(max(0.0, abs(uv.x)-0.25), uv.y-0.4)));
}

float uu(vec2 uv)
{
	return _u(uv, 0.25, 0.25);
}

float vv(vec2 uv)
{
	return line(vec2(abs(uv.x), uv.y), vec2(0.25, 0.4), vec2(0.0, -0.4));
}

float ww(vec2 uv)
{
	return min(line(vec2(abs(uv.x), uv.y), vec2(0.3, 0.4), vec2(0.2, -0.4)), line(vec2(abs(uv.x), uv.y), vec2(0.2, -0.4), vec2(0.0, 0.1)));
}

float xx(vec2 uv)
{
	return line(abs(uv), vec2(0.0, 0.0), vec2(0.3, 0.4));
}

float yy(vec2 uv)
{
	return min(line(uv, vec2(0.0, -0.2), vec2(-0.3, 0.4)), line(uv, vec2(0.3, 0.4), vec2(-0.3, -0.8)));
}

float zz(vec2 uv)
{
	return min(length(vec2(max(0.0, abs(uv.x)-0.25), abs(uv.y)-0.4)), line(uv, vec2(0.25, 0.4), vec2(-0.25, -0.4)));
}

// Uppercase
float AA(vec2 uv)
{
	return min(length(vec2(abs(length(vec2(uv.x, max(0.0, uv.y-0.35)))-0.25), min(0.0, uv.y+0.4))), length(vec2(max(0.0, abs(uv.x)-0.25), uv.y-0.1)));
}

float BB(vec2 uv)
{
	return min(length(vec2(abs(length(vec2(max(0.0, uv.x), abs(uv.y-0.1)-0.25))-0.25), min(0.0, uv.x+0.25))), length(vec2(uv.x+0.25, max(0.0, abs(uv.y-0.1)-0.5))));
}

float CC(vec2 uv)
{
	return uv.x<0.0||atan(uv.x, abs(uv.y-0.1)-0.25)<1.14?abs(length(vec2(uv.x, max(0.0, abs(uv.y-0.1)-0.25)))-0.25):min(length(vec2(uv.x+0.25, max(0.0, abs(uv.y-0.1)-0.25))), length(uv+vec2(-0.22734, -0.454)));
}

float DD(vec2 uv)
{
	return min(length(vec2(abs(length(vec2(max(0.0, uv.x), max(0.0, abs(uv.y-0.1)-0.25)))-0.25), min(0.0, uv.x+0.25))), length(vec2(uv.x+0.25, max(0., abs(uv.y-0.1)-0.5))));
}

float EE(vec2 uv)
{
	return min(min(length(vec2(max(0.0, abs(uv.x)-0.25), abs(uv.y-0.1))), length(vec2(max(0.0, abs(uv.x)-0.25), abs(uv.y-0.1)-0.5))), length(vec2(uv.x+0.25, max(0.0, abs(uv.y-0.1)-0.5))));
}

float FF(vec2 uv)
{
	return min(min(length(vec2(max(0.0, abs(uv.x)-0.25), uv.y-0.1)), length(vec2(max(0.0, abs(uv.x)-0.25), uv.y-0.6))), length(vec2(uv.x+0.25, max(0.0, abs(uv.y-0.1)-0.5))));
}

float GG(vec2 uv)
{
	float a=atan(uv.x, max(0.0, abs(uv.y-0.1)-0.25));
	return min(min(uv.x<0.0||a<1.14||a>3.0?abs(length(vec2(uv.x, max(0.0, abs(uv.y-0.1)-0.25)))-0.25):min(length(vec2(uv.x+0.25, max(0.0, abs(uv.y-0.1)-0.25))), length(uv+vec2(-0.22734, -0.454))), line(uv-vec2(0.0, 0.1), vec2(0.22734, -0.1), vec2(0.22734, -0.354))), line(uv-vec2(0.0, 0.1), vec2(0.22734, -0.1), vec2(0.05, -0.1)));
}

float HH(vec2 uv)
{
	return min(length(vec2(max(0.0, abs(uv.x)-0.25), uv.y-0.1)), length(vec2(abs(uv.x)-0.25, max(0.0, abs(uv.y-0.1)-0.5))));
}

float II(vec2 uv)
{
	return min(length(vec2(uv.x, max(0.0, abs(uv.y-0.1)-0.5))), length(vec2(max(0.0, abs(uv.x)-0.1), abs(uv.y-0.1)-0.5)));
}

float JJ(vec2 uv)
{
	return min(length(vec2(abs(length(vec2(uv.x+0.125, min(0.0, uv.y+0.15)))-0.25), max(0.0, max(-uv.x-0.125, uv.y-0.6)))), length(vec2(max(0.0, abs(uv.x)-0.125), uv.y-0.6)));
}

float KK(vec2 uv)
{
	return min(min(line(uv, vec2(-0.25, -0.1), vec2(0.25, 0.6)), line(uv, vec2(-0.1, 0.1), vec2(0.25, -0.4))), length(vec2(uv.x+0.25, max(0.0, abs(uv.y-0.1)-0.5))));
}

float LL(vec2 uv)
{
	return min(length(vec2(max(0.0, abs(uv.x)-0.2), uv.y+0.4)), length(vec2(uv.x+0.2, max(0.0, abs(uv.y-0.1)-0.5))));
}

float MM(vec2 uv)
{
	return min(min(min(length(vec2(uv.x-0.35, max(0.0, abs(uv.y-0.1)-0.5))), line(uv-vec2(0.0, 0.1), vec2(-0.35, 0.5), vec2(0.0, -0.1))), line(uv-vec2(0.0, 0.1), vec2(0.0, -0.1), vec2(0.35, 0.5))), length(vec2(uv.x+0.35, max(0.0, abs(uv.y-0.1)-0.5))));
}

float NN(vec2 uv)
{
	return min(min(length(vec2(uv.x-0.25, max(0.0, abs(uv.y-0.1)-0.5))), line(uv-vec2(0.0, 0.1), vec2(-0.25, 0.5), vec2(0.25, -0.5))), length(vec2(uv.x+0.25, max(0.0, abs(uv.y-0.1)-0.5))));
}

float OO(vec2 uv)
{
	return abs(length(vec2(uv.x, max(0.0, abs(uv.y-0.1)-0.25)))-0.25);
}

float PP(vec2 uv)
{
	return min(length(vec2(abs(length(vec2(max(0.0, uv.x), uv.y-0.35))-0.25), min(0.0, uv.x+0.25))), length(vec2(uv.x+0.25, max(0.0, abs(uv.y-0.1)-0.5))));
}

float QQ(vec2 uv)
{
	return min(abs(length(vec2(uv.x, max(0.0, abs(uv.y-0.1)-0.25)))-0.25), length(vec2(abs((uv.x-0.2)+(uv.y+0.3)), max(0.0, abs((uv.x-0.2)-(uv.y+0.3))-0.2)))/sqrt(2.0));
}

float RR(vec2 uv)
{
	return min(min(length(vec2(abs(length(vec2(max(0.0, uv.x), uv.y-0.35))-0.25), min(0.0, uv.x+0.25))), length(vec2(uv.x+0.25, max(0.0, abs(uv.y-0.1)-0.5)))), line(uv, vec2(0.0, 0.1), vec2(0.25, -0.4)));
}

float SS(vec2 uv)
{
	uv.y-=0.1;

	if(uv.y<0.275-uv.x*0.5&&uv.x>0.0||uv.y<-0.275-uv.x*0.5)
		uv=-uv;

	return atan(uv.x-0.05, uv.y-0.25)<1.14?abs(length(vec2(max(0.0, abs(uv.x)), uv.y-0.25))-0.25):length(vec2(uv.x-0.236, uv.y-0.332));
}

float TT(vec2 uv)
{
	return min(length(vec2(uv.x, max(0., abs(uv.y-0.1)-0.5))), length(vec2(max(0.0, abs(uv.x)-0.25), uv.y-0.6)));
}

float UU(vec2 uv)
{
	return length(vec2(abs(length(vec2(uv.x, min(0.0, uv.y+0.15)))-0.25), max(0.0, uv.y-0.6)));
}

float VV(vec2 uv)
{
	return line(vec2(abs(uv.x), uv.y), vec2(0.25, 0.6), vec2(0.0, -0.4));
}

float WW(vec2 uv)
{
	return min(line(vec2(abs(uv.x), uv.y), vec2(0.3, 0.6), vec2(0.2, -0.4)), line(vec2(abs(uv.x), uv.y), vec2(0.2, -0.4), vec2(0.0, 0.2)));
}

float XX(vec2 uv)
{
	return line(abs(vec2(uv.x, uv.y-0.1)), vec2(0.0, 0.0), vec2(0.3, 0.5));
}

float YY(vec2 uv)
{
	return min(min(line(uv, vec2(0.0, 0.1), vec2(-0.3, 0.6)), line(uv, vec2(0.0, 0.1), vec2(0.3, 0.6))), length(vec2(uv.x, max(0.0, abs(uv.y+0.15)-0.25))));
}

float ZZ(vec2 uv)
{
	return min(length(vec2(max(0.0, abs(uv.x)-0.25), abs(uv.y-0.1)-0.5)), line(uv, vec2(0.25, 0.6), vec2(-0.25, -0.4)));
}

// Numbers
float _11(vec2 uv)
{
	return min(min(line(uv, vec2(-0.2, 0.45), vec2(0.0, 0.6)), length(vec2(uv.x, max(0.0, abs(uv.y-0.1)-0.5)))), length(vec2(max(0.0, abs(uv.x)-0.2), uv.y+0.4)));
}

float _22(vec2 uv)
{
	return min(min(line(uv, vec2(0.185, 0.17), vec2(-0.25, -0.4)), length(vec2(max(0.0, abs(uv.x)-0.25), uv.y+0.4))), abs(atan(uv.x+0.025, uv.y-0.35)-0.63)<1.64?abs(length(uv+vec2(0.025, -0.35))-0.275):length(uv+vec2(0.255, -0.55)));
}

float _33(vec2 uv)
{
	uv.y=abs(uv.y-0.1)-0.25;
	return atan(uv.x, uv.y)>-1.0?abs(length(uv)-0.25):min(length(uv+vec2(0.211, -0.134)), length(uv+vec2(0.0, 0.25)));
}

float _44(vec2 uv)
{
	return min(min(length(vec2(uv.x-0.15, max(0.0, abs(uv.y-0.1)-0.5))), line(uv, vec2(0.15, 0.6), vec2(-0.25, -0.1))), length(vec2(max(0.0, abs(uv.x)-0.25), uv.y+0.1)));
}

float _55(vec2 uv)
{
	return min(min(length(vec2(max(0.0, abs(uv.x)-0.25), uv.y-0.6)), length(vec2(uv.x+0.25, max(0.0, abs(uv.y-0.36)-0.236)))), abs(atan(uv.x+0.1, uv.y+0.05)+1.57)<0.86&&uv.x+0.05<0.0?length(uv+vec2(0.3, 0.274)):abs(length(vec2(uv.x+0.05, max(0.0, abs(uv.y+0.1)-0.0)))-0.3));
}

float _66(vec2 uv)
{
	uv=-vec2(uv.x, uv.y-0.075);
	return min(abs(length(vec2(uv.x, max(0.0, abs(uv.y-0.175)-0.05)))-0.25), cos(atan(uv.x, uv.y-0.175+0.45)+0.65)<0.0||(uv.x>0.0&&uv.y<0.0)?abs(length(vec2(uv.x, max(0.0, abs(uv.y)-0.275)))-0.25):length(uv+vec2(0.2, 0.6)));
}

float _77(vec2 uv)
{
	return min(length(vec2(max(0.0, abs(uv.x)-0.25), uv.y-0.6)), line(uv, vec2(-0.25, -0.39), vec2(0.25, 0.6)));
}

float _88(vec2 uv)
{
	return min(abs(length(vec2(uv.x, abs(uv.y-0.1)-0.245))-0.255), length(vec2(max(0.0, abs(uv.x)-0.08), uv.y-0.1+uv.x*0.07)));
}

float _99(vec2 uv)
{
	return min(abs(length(vec2(uv.x, max(0.0, abs(uv.y-0.35)-0.05)))-0.25), cos(atan(uv.x, uv.y+0.1)+0.65)<0.0||(uv.x>0.0&&uv.y-0.35<0.0)?abs(length(vec2(uv.x, max(0.0, abs(uv.y-0.125)-0.275)))-0.25):length(uv+vec2(0.2, 0.25)));
}

float _00(vec2 uv)
{
	return abs(length(vec2(uv.x, max(0.0, abs(uv.y-0.1)-0.25)))-0.25);
}

// Special
float period(vec2 uv)
{
	return length(vec2(uv.x, uv.y+0.4))*0.97;
}

float comma(vec2 uv)
{
	return min(period(uv), line(uv, vec2(.031, -.405), vec2(-.029, -.52)));
}

float exclamation(vec2 uv)
{
	return min(period(uv), length(vec2(uv.x, max(0., abs(uv.y-.2)-.4)))-uv.y*.06);
}

float question(vec2 uv)
{
	return min(min(period(uv), length(vec2(uv.x, max(0.0, abs(uv.y+0.035)-0.1125)))), abs(atan(uv.x+0.025, uv.y-0.35)-1.05)<2.?abs(length(uv+vec2(0.025, -0.35))-.275):length(uv+vec2(0.25, -0.51))-.0);
}

float leftparenthesis(vec2 uv)
{
	return abs(atan(uv.x-0.62, uv.y)+1.57)<1.0?abs(length(uv-vec2(0.62, 0.0))-0.8):length(vec2(uv.x-0.185, abs(uv.y)-0.672));
}

float rightparenthesis(vec2 uv)
{
	return leftparenthesis(-uv);
}

float colon(vec2 uv)
{
	return length(vec2(uv.x, abs(uv.y-0.1)-0.25));
}

float semicolon(vec2 uv)
{
	return min(length(vec2(uv.x, abs(uv.y-0.1)-0.25)), line(uv-vec2(0.0, 0.1), vec2(0.0, -0.28), vec2(-0.029, -0.32)));
}

float _equal(vec2 uv)
{
	return length(vec2(max(0.0, abs(uv.x)-0.25), abs(uv.y-0.1)-0.15));
}

float plus(vec2 uv)
{
	return min(length(vec2(max(0.0, abs(uv.x)-0.25), uv.y-0.1)), length(vec2(uv.x, max(0.0, abs(uv.y-0.1)-0.25))));
}

float minus(vec2 uv)
{
	return length(vec2(max(0.0, abs(uv.x)-0.25), uv.y-0.1));
}

float asterisk(vec2 uv)
{
	uv=abs(vec2(uv.x, uv.y-0.1));
	return min(line(uv, vec2(0.866*0.25, 0.5*0.25), vec2(0.0)), length(vec2(uv.x, max(0.0, abs(uv.y)-0.25))));
}

float forwardslash(vec2 uv)
{
	return line(uv, vec2(-0.25, -0.4), vec2(0.25, 0.6));
}

float backslash(vec2 uv)
{
	return forwardslash(vec2(-uv.x, uv.y));
}

float less(vec2 uv)
{
	return line(vec2(uv.x, abs(uv.y-0.1)), vec2(0.25, 0.25), vec2(-0.25, 0.0));
}

float greater(vec2 uv)
{
	return less(vec2(-uv.x, uv.y));
}

float pound(vec2 uv)
{
	uv.y-=0.1;
	uv.x-=uv.y*0.1;
	uv=abs(uv);
	return min(length(vec2(uv.x-0.125, max(0.0, abs(uv.y)-0.3))), length(vec2(max(0.0, abs(uv.x)-0.25), uv.y-0.125)));
}

float ampersand(vec2 uv)
{
	uv+=vec2(0.05, -0.44);
	float x=min(min(abs(atan(uv.x, uv.y))<2.356?abs(length(uv)-0.15):1.0, line(uv, vec2(-0.106, -0.106), vec2(0.4, -0.712))), line(uv, vec2(0.106, -0.106), vec2(-0.116, -0.397)));
	uv+=vec2(-0.025, 0.54);
	return min(min(x, abs(atan(uv.x, uv.y)-0.785)>1.57?abs(length(uv)-0.2):1.0), line(uv, vec2(0.141, -0.141), vec2(0.377, 0.177)));
}

float pipe(vec2 uv)
{
	return length(vec2(uv.x, max(0.0, abs(uv.y-0.1)-0.5)));
}

float circumflex(vec2 uv)
{
	return less(-vec2(uv.y*1.5-0.7, uv.x*1.5-0.2));
}

float underline(vec2 uv)
{
	return length(vec2(max(0.0, abs(uv.x)-0.25), uv.y+0.4));
}

float leftsquare(vec2 uv)
{
	return min(length(vec2(uv.x+0.125, max(0.0, abs(uv.y-0.1)-0.5))), length(vec2(max(0.0, abs(uv.x)-0.125), abs(uv.y-0.1)-0.5)));
}

float rightsquare(vec2 uv)
{
	return leftsquare(vec2(-uv.x, uv.y));
}

float leftcurly(vec2 uv)
{
	return length(vec2(abs(length(vec2((uv.x*sign(abs(uv.y-0.1)-0.25)-0.2), max(0.0, abs(abs(uv.y-0.1)-0.25)-0.05)))-0.2), max(0.0, abs(uv.x)-0.2)));

}

float rightcurly(vec2 uv)
{
	return leftcurly(vec2(-uv.x, uv.y));
}

float sdfDistance(float dist)
{
	float weight=0.08;
	float px=fwidth(dist);
	return 1-smoothstep(weight-px, weight+px, dist);
}

// Missing characters: @$%^'"`~

void main()
{
	float dist=0.0;
	vec2 uv=vec2(UV.xy);

	switch(uint(UV.z))
	{
		case 33:	dist=exclamation(uv); break;
		case 35:	dist=pound(uv); break;
		case 36:	dist=0.0f; break;
		case 37:	dist=0.0f; break;
		case 38:	dist=ampersand(uv); break;
		case 40:	dist=leftparenthesis(uv); break;
		case 41:	dist=rightparenthesis(uv); break;
		case 42:	dist=asterisk(uv); break;
		case 43:	dist=plus(uv); break;
		case 44:	dist=comma(uv); break;
		case 45:	dist=minus(uv); break;
		case 46:	dist=period(uv); break;
		case 47:	dist=forwardslash(uv); break;

		case 48:    dist=_00(uv); break;
		case 49:    dist=_11(uv); break;
		case 50:    dist=_22(uv); break;
		case 51:    dist=_33(uv); break;
		case 52:    dist=_44(uv); break;
		case 53:    dist=_55(uv); break;
		case 54:    dist=_66(uv); break;
		case 55:    dist=_77(uv); break;
		case 56:    dist=_88(uv); break;
		case 57:    dist=_99(uv); break;

		case 58:	dist=colon(uv); break;
		case 59:	dist=semicolon(uv); break;
		case 60:	dist=less(uv); break;
		case 61:	dist=_equal(uv); break;
		case 62:	dist=greater(uv); break;
		case 63:	dist=question(uv); break;
		case 64:	dist=0.0; break;

		case 65:    dist=AA(uv); break;
		case 66:    dist=BB(uv); break;
		case 67:    dist=CC(uv); break;
		case 68:    dist=DD(uv); break;
		case 69:    dist=EE(uv); break;
		case 70:    dist=FF(uv); break;
		case 71:    dist=GG(uv); break;
		case 72:    dist=HH(uv); break;
		case 73:    dist=II(uv); break;
		case 74:    dist=JJ(uv); break;
		case 75:    dist=KK(uv); break;
		case 76:    dist=LL(uv); break;
		case 77:    dist=MM(uv); break;
		case 78:    dist=NN(uv); break;
		case 79:    dist=OO(uv); break;
		case 80:    dist=PP(uv); break;
		case 81:    dist=QQ(uv); break;
		case 82:    dist=RR(uv); break;
		case 83:    dist=SS(uv); break;
		case 84:    dist=TT(uv); break;
		case 85:    dist=UU(uv); break;
		case 86:    dist=VV(uv); break;
		case 87:    dist=WW(uv); break;
		case 88:    dist=XX(uv); break;
		case 89:    dist=YY(uv); break;
		case 90:    dist=ZZ(uv); break;

		case 91:	dist=leftsquare(uv); break;
		case 92:	dist=backslash(uv); break;
		case 93:	dist=rightsquare(uv); break;
		case 94:	dist=circumflex(uv); break;
		case 95:	dist=underline(uv); break;

		case 97:    dist=aa(uv); break;
		case 98:    dist=bb(uv); break;
		case 99:    dist=cc(uv); break;
		case 100:   dist=dd(uv); break;
		case 101:   dist=ee(uv); break;
		case 102:   dist=ff(uv); break;
		case 103:   dist=gg(uv); break;
		case 104:   dist=hh(uv); break;
		case 105:   dist=ii(uv); break;
		case 106:   dist=jj(uv); break;
		case 107:   dist=kk(uv); break;
		case 108:   dist=ll(uv); break;
		case 109:   dist=mm(uv); break;
		case 110:   dist=nn(uv); break;
		case 111:   dist=oo(uv); break;
		case 112:   dist=pp(uv); break;
		case 113:   dist=qq(uv); break;
		case 114:   dist=rr(uv); break;
		case 115:   dist=ss(uv); break;
		case 116:   dist=tt(uv); break;
		case 117:   dist=uu(uv); break;
		case 118:   dist=vv(uv); break;
		case 119:   dist=ww(uv); break;
		case 120:   dist=xx(uv); break;
		case 121:   dist=yy(uv); break;
		case 122:   dist=zz(uv); break;

		case 123:	dist=leftcurly(uv); break;
		case 124:	dist=pipe(uv); break;
		case 125:	dist=rightcurly(uv); break;
	};

	Output=vec4(Color.xyz, sdfDistance(dist));
}
