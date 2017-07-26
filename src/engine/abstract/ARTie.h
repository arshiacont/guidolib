#ifndef ARTie_H
#define ARTie_H

/*
  GUIDO Library
  Copyright (C) 2002  Holger Hoos, Juergen Kilian, Kai Renz
  Copyright (C) 2002-2017 Grame

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.

  Grame Research Laboratory, 11, cours de Verdun Gensoul 69002 Lyon - France
  research@grame.fr

*/


#include "ARBowing.h"

/** \brief not yet documented
*/
class ARTie : public ARBowing  
{
  	public:
				 	 ARTie() {}
      	virtual 	~ARTie() {}

	  	virtual bool MatchEndTag(const char * s);

	  	virtual void browse(TimeUnwrap& mapper) const;
		virtual const char*	getTagName() const		{ return "ARTie"; };
		virtual std::string getGMNName() const		{ return "\\tie"; };
};

#endif
