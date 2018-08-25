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

void GCode::dump(OutputStream &o) const
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
}

void GCode::dump(FILE *fp) const
{
	fprintf(fp, "%s%u", is_g?"G":is_m?"M":"", code);
	if(subcode != 0) {
		fprintf(fp, ".%u",  subcode);
	}
	fprintf(fp, " ");
	for(auto& i : args) {
		fprintf(fp, "%c%1.5f ", i.first, i.second);
	}
	fprintf(fp, "\n");
}

