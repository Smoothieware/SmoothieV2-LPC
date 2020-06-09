///////////////////////////////////////////////////////////////////////
//                                                                   //
//      Title:     Edinburgh Compatible Context Editor               //
//      Author:    H.Whitfield                                       //
//      Date:      17 November 1985,    last modified 29 July 1996   //
//                 Copyright (c) H.Whitfield 1985-1996               //
//                                                                   //
///////////////////////////////////////////////////////////////////////


/*
      Basic commands
      ==============
      m{n}                move to next line                       1
      m-{n}               move to previous line                   2
      f{m}'....'{n}       find text                            3,10
      s'....'             substitute text                         4
      g{n}                get line                                5
      k{n}                kill (delete) whole line                1
      i'....'{n}          insert text                             6
      b                   break line (insert newline)
      d{m}'....'{n}       delete text                           3,7
      j                   join (delete next newline)              6
      p{n}                print line                              1
      r{n}                move right                              8
      l{n}                move left                               9
      e{n}                erase right                             8
      e-{n}               erase left                              9
      t{m}'....'{n}       traverse text                         3,7
      u{m}'....'{n}       uncover (delete until) text           3,7
      v'....'             verify (test for) text

      Special commands
      ================
      %c                  close ( finish editing)
      %m                  normal monitoring (default)
      %f                  full monitoring
      %q                  quiet (no monitoring)

      Failure conditions and notes
      ============================

      1                   file pointer at end of file
      2                   current line is first line of file
      3                   text not found
      4                   no text to replace
      5                   entered line begins with a :
      6                   current line too long
      7                   default scope is current line
      8                   file pointer at end of current line
      9                   file pointer at beginning of current line
      10                  default scope is rest of file

      m                   scope (number of lines)
      n                   repetition number
      {}                  optional part of command

      Sequences of commands may be enclosed in  parentheses to form
      compound commands.

      0 or *              indefinite repetition
                          (repeat until failure) e.g.
             r0           move file pointer to end of line
             (mr)0        find first blank line
             e-0          erase backwards to start of line

      ?                   optional execution
                          (failure condition is ignored) e.g.
             ((r61p)?m)0  print lines with more than  60 characters

      ,                   alternative execution
                          (if first fails,  execute next and so on)
             (r81lb,m)0   split lines with more than  80 characters

             (f/print/(v/printstring/,e5i/write/))*         replace
                          print  by  write  except  in  printstring

      \                   inverted failure e.g.
             (mv'+'\)0    find next line starting with '+'
                                                                  x
      A  command  line  consisting  solely  of a number repeats the
      previous command line the number of times specified.

      Quotation marks     All characters except letters, digits and
                          those which have defined significance may
                          be used, e.g.   " / $ > + .
 */

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <functional>
#include <cstring>
#include <csetjmp>

#include "RingBuffer.h"

#pragma GCC diagnostic ignored "-Wshadow"

typedef char * PtrChar;
typedef PtrChar * PtrPtrChar;

typedef int boolean;

typedef char string[132];

namespace ecce {
const string version = "Ecce editor v3.3";
const char *usage="\
  Basic commands\n\
  ==============\n\
  m{n}                move to next line\n\
  m-{n}               move to previous line\n\
  f{m}/..../{n}       find text\n\
  s/..../             substitute text\n\
  g{n}                get line\n\
  k{n}                kill (delete) whole line\n\
  i/..../{n}          insert text\n\
  b                   break line (insert newline)\n\
  d{m}/..../{n}       delete text\n\
  j                   join (delete next newline)\n\
  p{n}                print line\n\
  r{n}                move right\n\
  l{n}                move left\n\
  e{n}                erase right\n\
  e-{n}               erase left\n\
  t{m}/..../{n}       traverse text\n\
  u{m}/..../{n}       uncover (delete until) text\n\
  v/..../             verify (test for) text\n\
\n\
  Special commands\n\
  ================\n\
  %h                  help\n\
  %c                  close ( finish editing)\n\
  %m                  normal monitoring (default)\n\
  %f                  full monitoring\n\
  %q                  quiet (no monitoring)\n\
";
std::function<void(char)> _outc;

std::jmp_buf jump_buffer;
RingBuffer<char> inbuf;

const int amax = 4096;  		//  size of internal text buffer
const int argmax = 64;
const int cmax   = 121;
const int stop   = -5000;
const int inv    = -5001;
const int lmax   = 2048;  		// maximum line length

const int firstcol   = 1;
const int lastcol    = 160;
const int linelength = 160;
const int parslength = 20;
const int tbase      = 1;

const char nl   = char(10);		// Unix eol
const char cr   = char(13);		// Macintosh eol
const char tab  = char(9);		// Ascii tab

int top, pe, pp, fp, bottom, pp1, ms, ml, lim, p, ci, ti, txt, clim;
int num, codelim, matchlim, arg, mon, itype, chain;
char code, last, term, ch, sym, quote;
bool printed, okok, done, failed, detab, again, errorFlag;

char *a= NULL;
int  *c= NULL;

void halt (const string s);					// prototypes

void writestring(const string s);
void writeln();

int _inbyte();

///////////////////////////////////////////////////////////////////////
//                                                                   //
//      I/O Interface Functions  File I/O                            //
//                                                                   //
///////////////////////////////////////////////////////////////////////

std::ifstream inFile;
std::ofstream outFile;

char inFileBuffer;						// one char look-ahead buffer
boolean inFileEmpty = true;

void openFiles(const char *inf, const char *outf)
{
	inFile.open(inf, std::ios::in);
	if ( ! inFile )
	{
		halt("Open inFile failed");
	}

	outFile.open(outf, std::ios::out);
	if ( ! outFile )
	{
		halt("Open outFile failed");
	}
}

boolean eofInFile()
{
	if ( inFileEmpty )					// if look-ahead buffer empty
	{
		return inFile.eof();			// return the real eof
	}

	return false;						// else return false
}

char nextInFile()						// does not move past char
{
	if ( inFileEmpty )					// if look-ahead buffer empty
	{									// try to get the next char
		if ( inFile.eof() )
		{
			halt("Eof encountered on input file.");
		}
		else
		{
			inFile.get( inFileBuffer );
			inFileEmpty = false;		// look-ahead buffer now full
		}
	}

	return inFileBuffer;
}

void readInFile(char& c)				// gets and moves past char
{
	c = nextInFile();					// get the next char
	inFileEmpty = true;					// empty the look-ahead buffer
}

void putOutFile(char c)					// puts one char on output file
{
		outFile << c;
}

void closeFiles()						// closes output file
{
	outFile.close();
	inFile.close();
}


///////////////////////////////////////////////////////////////////////
//                                                                   //
//      I/O Interface Functions  Program Control                     //
//                                                                   //
///////////////////////////////////////////////////////////////////////

void halt (const string s)
{
	writestring(s);
	writeln();
	closeFiles();
	std::longjmp(jump_buffer, 1);
}


///////////////////////////////////////////////////////////////////////
//                                                                   //
//      I/O Interface Functions  Keyboard/Screen                     //
//                                                                   //
///////////////////////////////////////////////////////////////////////

void write(const char c)
{
	_outc(c);
}

void writeln()
{
	_outc('\n');
}

void writestring(const string s)
{
	for (size_t i = 0; i < strlen(s); ++i) {
		_outc(s[i]);
	}
}

char inputBuffer;						// one char look-ahead buffer
boolean inputEmpty = true;

char lastsym, prompt;					// prompt management variables
boolean prompted;


char nextsymbol()						// does not move past char
{
	if ( (lastsym == nl) && (! prompted) )
	{
		if ( prompt != ' ' )
		{
			write(prompt);
		}
		prompted = true;
	}

	if ( inputEmpty )					// if look-ahead buffer empty
	{
		inputBuffer= _inbyte();							// try to get the next char
		if ( inputBuffer == 0 )
		{
			halt("Eof encountered on keyboard input.");
		}
		else
		{
			inputEmpty = false;			// look-ahead buffer now full
		}
	}

	return inputBuffer;
}

char readsymbol()						// gets and moves past char
{
	lastsym = nextsymbol();				// get the next char

	if ( lastsym == nl  )
	{
		prompted = false;
	}

	inputEmpty = true;					// empty the look-ahead buffer
	return lastsym;
}


///////////////////////////////////////////////////////////////////////
//                                                                   //
//      Body of Ecce Editor                                          //
//                                                                   //
///////////////////////////////////////////////////////////////////////

char lower (char ch)
{
	if ( ('A' <= ch)  && (ch <= 'Z') )
	{
		return  char(int(ch) - int('A') + int('a'));
	}
	else if ( ch == '`'  )
	{
		return '@';
	}
	else if ( ch == '{'  )
	{
		return '[';
	}
	else if ( ch == '|'  )
	{
		return '\\';
	}
	else if ( ch == '}'  )
	{
		return ']';
	}
	else if ( ch == '~'  )
	{
		return '^';
	}
	else
	{
		return ch;
	}
}


void readnum()
{
	char ch;

	num = int(sym) - int('0');
	ch = nextsymbol();
	while ( ('0' <= ch) && (ch <= '9') )
	{
		num = 10 * num + int(ch) - int('0');
		readsymbol();
		ch = nextsymbol();
	}
}


int nextitemtype()
{
	int result;

	do
	{
		sym = readsymbol();
	} while ( ! (  sym != ' ' ) );

	sym = lower(sym);

	if ( sym < ' '  )
	{
		result = 1;
	}
	else
	{
		switch ( sym )
		{
			case ';':
				result = 1;
				break;
			case '(':
				result = 2;
				break;
			case ',':
				if ( nextsymbol() == nl )
				{
					readsymbol();
				};
				result = 3;
				break;
			case ')':
				result = 4;
				break;
			case 'i':
			case 's':
				result = 5;
				break;
			case 'd':
				result = 6;
				break;
			case 'f':
			case 't':
			case 'u':
				result = 7;
				break;
			case 'v':
				result = 8;
				break;
			case 'e':
			case 'm':
				if ( nextsymbol() == '-' )
				{
					readsymbol();
					if ( sym == 'e' )
					{
						sym = 'o';
					}
					else
					{
						sym = 'w';
					}
				};
				result = 9;
				break;
			case 'b':
			case 'g':
			case 'j':
			case 'k':
			case 'l':
			case 'p':
			case 'r':
				result = 9;
				break;
			case 'a':
			case 'c':
			case 'h':
			case 'n':
			case 'o':
			case 'q':
			case '-':
			case 'w':
			case 'x':
			case 'y':
			case 'z':
				result = 10;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				readnum();
				result = 0;
				break;
			case '*':
				num = 0;
				result = 0;
				break;
			case '?':
				num = stop + 1;
				result = 0;
				break;
			case '\\':
			case '^':
				num = inv + 1;
				result = 0;
				break;
			default:
				// '!', '"', '#', '$', '%', '&', '''', '+', '.', '/', ':', '<', '=', '>', '@', '[', ']', '_':
				result = -1;
				break;
		}	// switch

	}	// else
	return result;
}


void unchain()
{
	boolean finished;
	txt = chain;
	if ( txt != 0 )
	{
		finished = false;
		do
		{
			chain = c[txt];
			c[txt] = ci;
			if ( c[txt + 1] != int('y') )
			{
				txt = chain;
			}
			else
			{
				finished = true;
			}
		} while ( ! (  (txt == 0) || finished ) );
	}
}


void stack (int v)
{
	ci = ci - 1;
	c[ci] = v;
}


void push()
{
	stack(256 * matchlim + int(code));
	stack(txt);
	stack(num);
}


void procerror (int n)
{
	if ( n != 6  )
	{
		switch ( n )
		{
			case 0:
				write(' ');
				write(code);
				code = sym;
				break;
			case 1:
				code = sym;
				break;
			case 2:
				code = '(';
				break;
			case 3:
				writestring(" text for");
				break;
			case 5:
				break;
		}
		write(' ');
		write(code);
		write('?');
		writeln();
	}
	else
	{
		writestring(" too long");
		writeln();
	};
	if ( ci != cmax  )
	{
		clim = 0;
	}
	while ( sym != nl )
	{
		sym = readsymbol();
	}
		errorFlag = true;
}


void qstring()
{
	if ( (itype >= 0) || (txt != 0) )
	{
		procerror(3);
	}
	else
	{
		quote = sym;
		txt = ti;
		while ( (nextsymbol() != quote) && (nextsymbol() != nl) && (! errorFlag) )
		{
			sym = readsymbol();
			if ( detab  )
			{
				if ( sym == tab )
				{
					sym = ' ';
				}
			}
			c[ti] = int(sym);
			ti = ti + 1;
			if ( ti == ci  )
			{
				procerror(6);
			}
		}
		if ( ! errorFlag  )
		{
			if ( nextsymbol() == nl )
			{
				if ( (code != 'i') && (code != 's') )
				{
					procerror(3);
				}
			}
			else
			{
				sym = readsymbol();
			}
			if ( ! errorFlag  )
			{
				if ( (ti == txt) && (code != 's')  )
				{
					procerror(3);
				}
				else
				{
					c[ti] = 0;
					ti = ti + 1;
					itype = nextitemtype();
					if ( itype == 0  )
					{
						itype = nextitemtype();
					}
					push();
				}
			}
		}
	}
}


void readcommand()
{
	int i;
	done = false;
	do
	{
		again = false;
		errorFlag = false;
		prompt = '>';
		do
		{
			itype = nextitemtype();
		} while ( ! ( itype != 1 ) );
		ci = cmax;
		ti = tbase;
		chain = 0;

		if ( (itype == 0) && (clim != 0) ) 	// repeat last command
		{
			c[clim] = num;
			if ( nextitemtype() == 1 )
			{
				done = true;
			}
			else
			{
				procerror(1);
			}
		}
		else if ( sym == '%')
		{
			sym = lower(readsymbol());
			code = sym;
			matchlim = 0;
			num = 0;
			txt = 0;
			itype = nextitemtype();
			if ( itype != 1 )
			{
				procerror(1);
			}
			else if ( code == 'c' )
			{
				push();
			}
			else if(code == 'h')
			{
				writestring(usage);
				writeln();
			}
			else if ( (code == 'q') || (code == 'm') || (code == 'f') )
			{
				mon = int('m') - int(code);
				again = true;
				switch ( code )
				{
					case'q':
						writestring("quiet mode");
						writeln();
						break;
					case 'm':
						writestring("normal mode");
						writeln();
						break;
					case 'f':
						writestring("full mode");
						writeln();
						break;
				}
			}
			else if ( code == 't' )
			{
				detab = ! detab;
				if ( detab )
				{
					writestring("detab on");
					writeln();
				}
				else
				{
					writestring("detab off");
					writeln();
				}
			}
			else
			{
				procerror(1);
			}
		}
		else
		{
			do
			{
				if ( itype <= 0 )
				{
					procerror(1);
				}
				else if ( ci - 4 <= ti )
				{
					procerror(6);
				}
				else
				{
					code = sym;
					matchlim = 0;
					txt = 0;
					if ( code == 'f' )
					{
						num = 0;
					}
					else
					{
						num = 1;
					}
					i = itype;
					itype = nextitemtype();
					switch ( i )
					{

						case 2:		// left bracket
							code = 'y';
							txt = chain;
							chain = ci - 2;
							push();
							break;

						case 3: 	// comma
							num = inv;
							code = 'z';
							txt = chain;
							chain = ci - 2;
							push();
							break;

						case 4: 	// right bracket
							unchain();
							if ( txt == 0  )
							{
								procerror(5);
							}
							else
							{
								c[txt] = ci - 3;
								txt = txt - 1;
								c[txt] = num;
								code = 'z';
								if ( itype == 0  )
								{
									itype = nextitemtype();
								}
								push();
							}
							break;

						case 5: 	// insert,substitute
							qstring();
							break;

						case 6:		// delete
						case 7:		// find,traverse,uncover
							matchlim = num;
							num = 1;
							if ( itype == 0 )
							{
								itype = nextitemtype();
							}
							qstring();
							break;

						case 8: 	// verify
							qstring();
							break;

						case 9:
							if ( itype < 0 )
							{
								procerror(0);
							}
							else     // all the others
							{
								if ( itype == 0 )
								{
									itype = nextitemtype();
								}
								push();
							}
							break;

						case 10: 	// invalid letters
							procerror(5);
							break;
					} // switch
				} // else
			} while ( ! (  (itype == 1) || errorFlag ) );
		}

		if ( (! done) && (! again) && (! errorFlag) )
		{
			unchain();
			if ( txt != 0 )
			{
				procerror(2);
			}
			else
			{
				stack(int('z'));
				stack(cmax);
				stack(1);
				clim = ci;
				stack(0);
				done = true;
			}
		}
	} while ( ! ( done ) );
}


void makespace()
{
	char k;
	int p1, p2;

	if ( fp - pp - 240 <= 0 )
	{
		p1 = top;
		if ( code == 'c' )
		{
			p2 = pe;
		}
		else
		{
			p2 = (p1 + pe) / 2;
		}
		if ( p2 == top )
		{
			halt("Fatal error in makespace.");
		}
		do
		{
			do
			{
				k = a[p1];
				putOutFile(k);
				p1 = p1 + 1;
			} while ( ! ( (k == nl) ) );
		} while ( ! ( (p1 - p2 >= 0) ) );
		pe = top + pe - p1;
		p2 = pp;
		pp = top;
		while ( p1 != p2 )
		{
			a[pp] = a[p1];
			pp = pp + 1;
			p1 = p1 + 1;
		}
	}
}


void printline()
{
	int p;

	printed = true;
	if ( fp == bottom )
	{
		writestring("**end**");
		writeln();
	}
	else
	{
		if ( pe == pp )
		{
			p = fp;
		}
		else
		{
			p = pe;
		}
		if ( a[p] != nl  )
		{
			write(a[p]);
		}
		else
		{
			writeln();
		}
		while ( a[p] != nl )
		{
			p = p + 1;
			if ( (p == pp) && (num == 0) )
			{
				write('^');
			}
			if ( p == pp )
			{
				p = fp;
			}
			if ( a[p] != nl  )
			{
				write(a[p]);
			}
			else
			{
				writeln();
			}
		}
	}
}


void readline()
{
	char k;

	printed = false;
	if ( fp == bottom )
	{
		fp = lim - lmax;
		ms = 0;
		if ( eofInFile() )
		{
			fp = lim;
			bottom = fp;
			a[fp] = nl;
		}
		else
		{
			do
			{
				readInFile(k);
				if ( detab )
				{
					if ( k == tab )
					{
						k = ' ';
					}
				}
				a[fp] = k;
				fp = fp + 1;
			} while ( ! (  (k == nl) || eofInFile() || (fp == lim) ) );

			if ( k == nl )
			{
				bottom = fp;
				fp = lim - lmax;
			}
			else if ( eofInFile() )  	// unexpected eof before eoln
			{
				a[fp] = nl;
				fp = fp + 1;
				bottom = fp;
				fp = lim - lmax;
			}
			else
			{
				if ( nextInFile() == nl )
				{
					readInFile(k);
				}
				a[fp] = nl;
				fp = fp + 1;
				bottom = fp;
				fp = lim - lmax;
			}
		}
	}
}


void lefttab()
{
	while ( pp != pe )
	{
		fp = fp - 1;
		pp = pp - 1;
		a[fp] = a[pp];
	}
}


void move()
{
	char k;
	makespace();
	do
	{
		k = a[fp];
		a[pp] = k;
		pp = pp + 1;
		fp = fp + 1;
	} while ( ! (  k == nl ) );
	pe = pp;
	readline();
}


void moveback()
{
	char k;

	k = a[pp - 1];
	while ( (k != nl) || (pp == pe) )
	{
		fp = fp - 1;
		pp = pp - 1;
		a[fp] = k;
		k = a[pp - 1];
	};
	pe = pp;
	ms = 0;
	printed = false;
}


boolean matched()
{
	int i, l, ind, t1;
	char k;
	int fp1;

	pp1 = pp;
	fp1 = fp;
	ind = matchlim;
	t1 = c[txt];
	if ( (fp != ms) || ((code != 'f') && (code != 'u'))  )
	{
		goto L2;
	}
	k = a[fp];

L1:
	a[pp] = k;
	pp = pp + 1;
	fp = fp + 1;

L2:
	k = a[fp];
	if ( k == char(t1)  )
	{
		goto L5;
	}
	if ( k != nl  )
	{
		goto L1;
	}
	else
	{
		goto L10;
	}

L5:
	l = 1;

L6:
	i = c[txt + l];
	if ( i == 0  )
	{
		goto L7;
	}
	if ( a[fp + l] != char(i) )
	{
		goto L1;
	}
	l = l + 1;
	goto L6;

L7:
	ms = fp;
	ml = fp + l;
	return true;

L10:
	ind = ind - 1;
	if ( ind == 0  )
	{
		goto L15;
	}
	if ( fp == bottom  )
	{
		goto L16;
	}
	if ( code != 'u'  )
	{
		a[pp] = k;
		pp = pp + 1;
		pe = pp;
	}
	else
	{
		pp = pp1;
	}
	fp = fp + 1;
	makespace();
	readline();
	pp1 = pp;
	fp1 = fp;
	goto L2;

L15:
	pp = pp1;
	fp = fp1;

L16:
	return false;
}


void fail()
{
	writestring("failure: ");
	if ( code == 'o' )
	{
		write('e');
		code = '-';
	}
	else if ( code == 'w' )
	{
		write('m');
		code = '-';
	}
	if ( code != 'z' )
	{
		write(code);
		if ( txt > 0  )
		{
			write('\'');
			while ( c[txt] != 0 )
			{
				write(char(c[txt]));
				txt = txt + 1;
			}
			write('\'');
		}
	}
	if ( num == inv  )
	{
		write('\\');
	}
	writeln();
}


void insert()
{
	int i;

	makespace();
	if ( (pp - pe > linelength) || (fp == bottom) )
	{
		okok = false;
	}
	else
	{
		i = txt;
		while ( c[i] != 0 )
		{
			a[pp] = char(c[i]);
			pp = pp + 1;
			i = i + 1;
		}
	}
}

int main(const char *infile, const char *outfile, std::function<void(char)> outfnc)
{
	int i, k;
	bool doexit= false;

	_outc= outfnc;

	a= (char *)malloc(amax+1);
	if(a == NULL) {
		writestring("error: a out of memory\n");
		return 1;
	}
	c= (int *)malloc((cmax+1) * sizeof(int));
	if(c == NULL) {
		free(a);
		writestring("error: c out of memory\n");
		return 1;
	}

	const size_t bufsz= 132;
	char *buffer= (char *)malloc(bufsz);
	inbuf.allocate(buffer, bufsz);
	if(!inbuf.is_ok()) {
		writestring("error: inbuf out of memory\n");
		free(a);
		free(c);
		return 1;
	}

	if (setjmp(jump_buffer) == 1) {
		doexit= true;
		if(a != NULL) free(a);
		if(c != NULL) free(c);
		inbuf.deallocate();
		free(buffer);
        return 1;
    }

    openFiles(infile, outfile);

	lastsym = char(0);
	prompted = false;

	mon = 0;
	detab = false;
	printed = false;
	fp = 0;
	bottom = 0;
	ms = 0;
	ml = 0;
	top = 1;
	lim = amax;
	clim = 0;
	pp = top - 1;
	a[pp] = nl;
	pp = pp + 1;
	pe = pp;

	writestring(version);
	writeln();
	write('>');
	readline();

	do
	{
		failed = false;
		readcommand();
		term = sym;
		ci = cmax;
		last = char(0);
		codelim = c[ci - 1];

		while ( (codelim != 0) && (! failed) )
		{
			code = char(codelim & 255);
			matchlim = codelim / 256;
			txt = c[ci - 2];
			num = c[ci - 3];
			ci = ci - 3;
			done = false;
			okok = true;
			do
			{
				num = num - 1;
				switch ( code ) 	// 'a' to 'z'
				{
					case 'a':
						break; 		// dummy

					case 'b':
						a[pp] = nl;
						pp = pp + 1;
						pe = pp;
						break;

					case 'c':
						while ( fp != bottom )
						{
							move();
						}
						while ( top != pp )
						{
							putOutFile(a[top]);
							top = top + 1;
						};
						closeFiles();
						// deallocate memory
						free(a);
						free(c);
						inbuf.deallocate();
						free(buffer);
						return 0;

					case 'd':
						okok = matched();
						if ( okok  )
						{
							fp = ml;
						}
						break;

					case 'e':
						if ( a[fp] == nl )
						{
							okok = false;
						}
						else
						{
							fp = fp + 1;
						}
						break;

					case 'f':
						okok = matched();
						break;

					case 'g':
						if ( prompt == '>'  )
						{
							prompt = ':';
						}
						else
						{
							prompt = ' ';
						}
						makespace();
						sym = readsymbol();
						if ( detab  )
						{
							if ( sym == tab )
							{
								sym = ' ';
							}
						}
						if ( sym == ':' )
						{
							okok = false;
						}
						else
						{
							lefttab();
							a[pp] = sym;
							pp = pp + 1;
							pe = pp;
							while ( sym != nl )
							{
								sym = readsymbol();
								a[pp] = sym;
								pp = pp + 1;
								pe = pp;
							}
						}
						break;

					case 'h':
						break;

					case 'i':
						insert();
						break;

					case 'j':
						if ( fp == bottom  )
						{
							okok = false;
						}
						else
						{
							do
							{
								ch = a[fp];
								a[pp] = ch;
								pp = pp + 1;
								fp = fp + 1;
							} while ( ! (  ch == nl ) );
							readline();
							pp = pp - 1;
							if ( (pp - pe > linelength) || ((fp == bottom) && (pp != pe)) )
							{
								pp = pp + 1;
								pe = pp;
								okok = false;
							}
						}
						break;

					case 'k':
						if ( fp == bottom  )
						{
							okok = false;
						}
						else
						{
							pp = pe;
							do
							{
								fp = fp + 1;
							} while ( ! (  a[fp - 1] == nl ) );
							readline();
						}
						break;

					case 'l':
						if ( pp == pe  )
						{
							okok = false;
						}
						else
						{
							fp = fp - 1;
							pp = pp - 1;
							a[fp] = a[pp];
							ms = 0;
						}
						break;

					case 'm':
						if ( fp == bottom  )
						{
							okok = false;
						}
						else
						{
							move();
						}
						break;

					case 'n':
						break;		// dummy

					case 'o':
						if ( pp == pe  )
						{
							okok = false;
						}
						else
						{
							pp = pp - 1;
						}
						break;

					case 'p':
						if ( last != 'p'  )
						{
							printline();
						}
						else if ( fp == bottom  )
						{
							okok = false;
						}
						else
						{
							move();
							printline();
						}
						break;

					case 'q':
						break;		// dummy

					case 'r':
						ch = a[fp];
						if ( ch == nl )
						{
							okok = false;
						}
						else
						{
							a[pp] = ch;
							pp = pp + 1;
							fp = fp + 1;
						}
						break;

					case 's':
						if ( fp != ms )
						{
							okok = false;
						}
						else
						{
							fp = ml;
							insert();
						}
						break;

					case 't':
						if ( ! matched()  )
						{
							okok = false;
						}
						else
						{
							fp = ml;
							insert();
						}
						break;

					case 'u':
						if ( ! matched()  )
						{
							okok = false;
						}
						else
						{
							pp = pp1;
						}
						break;

					case 'v':
						p = fp;
						i = txt;
						k = c[i];
						while ( (k != 0) && okok )
						{
							if ( a[p] != char(k) )
							{
								okok = false;
							}
							else
							{
								p = p + 1;
								i = i + 1;
								k = c[i];
							}
						}
						if ( okok  )
						{
							ms = fp;
							ml = p;
						}
						break;

					case 'w':
						makespace();
						if ( pe == top  )
						{
							okok = false;
						}
						else
						{
							moveback();
						}
						break;

					case 'x':
						break;		// dummy

					case 'y':
						c[txt] = num + 1;
						done = true;
						break;

					case 'z':
						if ( num == inv  )
						{
							okok = false;
						}
						else
						{
							if ( (num != 0) && (num != stop) )
							{
								c[ci] = num;
								ci = txt;
							}
						}
						done = true;
						break;

				}; // switch

				if ( okok && (! done) )
				{
					last = code;
				}

			} while ( ! ( (num == 0) || (num == stop) || (num == inv) || done || ! okok) );

			if ( ((okok != done) && (num == inv)) || ! (done || okok || (num < 0)) )
			{
				do
				{
					k = c[ci - 1];
					ci = ci - 3;
					if ( char(k) == 'y'  )
					{
						ci = c[ci + 1];
					}
				} while ( ! ( (k == 0) || ((char(k) == 'z') && (c[ci] <= 0)) ) );
				if ( k == 0 )
				{
					fail();
					failed = true;
				}
			};

			if ( ! failed  )
			{
				codelim = c[ci - 1];
			}

		};

		if ( term == nl  )
		{
			num = 0;
			if ( ((mon == 0) && (! printed)) || ((mon > 0) && (last != 'p')) )
			{
				printline();
			}
		}

	} while (!doexit); // forever

	free(a);
	free(c);
	inbuf.deallocate();
	free(buffer);
	return 1;
}

#include "FreeRTOS.h"
#include "task.h"

void add_input(char c)
{
	while(inbuf.full()) {
		vTaskDelay(pdMS_TO_TICKS(10));
	}
	inbuf.push_back(c);
}

int _inbyte()
{
	while (inbuf.empty()) {
		vTaskDelay(pdMS_TO_TICKS(1));
	}
	int c= inbuf.pop_front();
	if(c == 3) c= 0; // ctrl c terminates
	return c;
}

}
