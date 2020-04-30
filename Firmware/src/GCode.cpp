#include "GCode.h"

#include "OutputStream.h"

GCode::GCode()
{
	clear();
}

void GCode::clear()
{
	is_g= false;
	is_m= false;
	is_t= false;
	is_modal= false;
	is_immediate= false;
	is_error= false;
	error_message= nullptr;
	argbitmap= 0;
	args.clear();
	code= subcode= 0;
}

bool GCode::dump(OutputStream &o) const
{
	o.printf("%s%u", is_g?"G":is_m?"M":"", code);
	if(subcode != 0) {
		o.printf(".%u",  subcode);
	}
	o.printf(" ");
	for(auto& i : args) {
		o.printf("%c:%1.5f ", i.first, i.second);
	}
	o.printf("\n");
	return true;
}

bool GCode::dump(FILE *fp) const
{
	if(fprintf(fp, "%s%u", is_g?"G":is_m?"M":"", code) < 0) return false;
	if(subcode != 0) {
		if(fprintf(fp, ".%u",  subcode) < 0) return false;
	}
	if(fprintf(fp, " ") < 0) return false;
	for(auto& i : args) {
		if(fprintf(fp, "%c%1.5f ", i.first, i.second) < 0) return false;
	}
	if(fprintf(fp, "\n") < 0) return false;

	return true;
}

