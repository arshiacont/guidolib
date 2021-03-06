#ifndef ARRitardando_H
#define ARRitardando_H

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

#include "ARFontAble.h"
#include "ARPositionTag.h"

class TagParameterString;

/** \brief Ritardando tag
*/

class ARRitardando : public ARFontAble,  public ARPositionTag
{		
	public:			

		virtual bool MatchEndTag(const char * s);

						 ARRitardando();
		virtual 		~ARRitardando() {}

		virtual void setTagParameters (const TagParameterMap& params);

		virtual const char*	getParamsStr() const	{ return kARRitardandoParams; };
		virtual const char*	getTagName() const		{ return "ARRitardando"; };
		virtual std::string getGMNName() const		{ return "\\ritardando"; };

		const TagParameterString * getTempo() const       { return tempo; }
		const TagParameterString * getAbsTempo() const    { return abstempo; }

	protected:
		const TagParameterString *tempo;
		const TagParameterString *abstempo;
};

#endif
