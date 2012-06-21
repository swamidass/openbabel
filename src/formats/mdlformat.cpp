/**********************************************************************
Copyright (C) 1998-2001 by OpenEye Scientific Software, Inc.
Some portions Copyright (C) 2001-2006 by Geoffrey Hutchison
Portions Copyright (C) 2004-2006 by Chris Morley

This file is part of the Open Babel project.
For more information, see <http://openbabel.org/>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
***********************************************************************/
#include <openbabel/babelconfig.h>

#ifdef _WIN32
#pragma warning (disable : 4786)
#endif

#include <ctime>
#include <vector>
#include <iomanip>
#include <map>
#include <algorithm>
#include <openbabel/obmolecformat.h>
#include <openbabel/stereo/stereo.h>
#include <openbabel/stereo/cistrans.h>
#include <openbabel/stereo/tetrahedral.h>
#include <openbabel/alias.h>
#include <openbabel/tokenst.h>
#include <openbabel/atomclass.h>

using namespace std;
namespace OpenBabel
{

  //MDLFormat is a base class which is never instantiated.
  //MOLFormat and SDFormat are derived from it and have their own constructors.
  //SDFormat has its own WriteMolecule which sets the "sd" option.

  class MDLFormat : public OBMoleculeFormat
  {
    public:
      virtual const char* Description()
      {
        return "MDL MOL format\n"
               "Reads and writes V2000 and V3000 versions\n\n"
               "Read Options, e.g. -as\n"
               " s  determine chirality from atom parity flags\n"
               "       The default setting is to ignore atom parity and\n"
               "       work out the chirality based on the bond\n"
               "       stereochemistry.\n"
               " T  read title only\n"
               " P  read title and properties only\n"
               "       When filtering an sdf file on title or properties\n"
               "       only, avoid lengthy chemical interpretation by\n"
               "       using the ``T`` or ``P`` option together with the\n"
               "       :ref:`copy format <Copy_raw_text>`.\n\n"

               "Write Options, e.g. -x3\n"
               " 3  output V3000 not V2000 (used for >999 atoms/bonds) \n"
               " m  write no properties\n"
               " w  use wedge and hash bonds from input (2D only)\n"
               " A  output in Alias form, e.g. Ph, if present\n\n";
      }

      virtual const char* SpecificationURL()
      {
        return "http://www.mdl.com/downloads/public/ctfile/ctfile.jsp";
      }

      virtual const char* GetMIMEType()
      {
        return "chemical/x-mdl-molfile";
      }

      virtual unsigned int Flags() { return DEFAULTFORMAT | ZEROATOMSOK; }
      virtual const char* TargetClassDescription() { return OBMol::ClassDescription(); }

      virtual int SkipObjects(int n, OBConversion* pConv)
      {
        if (n == 0)
          n++;
        istream& ifs = *pConv->GetInStream();
        do {
         ignore(ifs, "$$$$\n");
        } while(ifs && --n);
        return ifs.good() ? 1 : -1;
      }

      ////////////////////////////////////////////////////
      /// The "API" interface functions
      virtual bool ReadMolecule(OBBase* pOb, OBConversion* pConv);
      virtual bool WriteMolecule(OBBase* pOb, OBConversion* pConv);

      ////////////////////////////////////////////////////
      //V3000 routines
    private:
      bool ReadV3000Block(istream& ifs, OBMol& mol, OBConversion* pConv,bool DoMany);
      bool ReadV3000Line(istream& ifs, vector<string>& vs);
      bool ReadAtomBlock(istream& ifs,OBMol& mol, OBConversion* pConv);
      bool ReadBondBlock(istream& ifs,OBMol& mol, OBConversion* pConv);
      bool ReadRGroupBlock(istream& ifs,OBMol& mol, OBConversion* pConv);
      bool ReadUnimplementedBlock(istream& ifs,OBMol& mol, OBConversion* pConv, string& blockname);
      bool WriteV3000(ostream& ofs,OBMol& mol, OBConversion* pConv);
      void ReadPropertyLines(istream& ifs, OBMol& mol);
      bool TestForAlias(const string& symbol, OBAtom* at, vector<pair<AliasData*,OBAtom*> >& aliases);

    private:
      enum Parity {
        NotStereo, Clockwise, AntiClockwise, Unknown
      };
      bool  HasProperties;
      string GetTimeDate();
      void GetParity(OBMol& mol, map<OBAtom*, Parity> &parity);
      void TetStereoFromParity(OBMol& mol, vector<MDLFormat::Parity> &parity, bool deleteExisting=false);
      int ReadIntField(const char *s);
      unsigned int ReadUIntField(const char *s);
     // Helper for 2.3 -- is this atom a metal
      bool IsMetal(OBAtom *atom);// Temporary for 2.3.1 (because of binary compatibility)
      map<int,int> indexmap; //relates index in file to index in OBMol
      vector<string> vs;
  };

  //**************************************
  class MOLFormat : public MDLFormat
  {
    public:
      //Register this format type ID
      MOLFormat()
      {
        OBConversion::RegisterFormat("mol",this, "chemical/x-mdl-molfile");
        OBConversion::RegisterFormat("mdl",this, "chemical/x-mdl-molfile");
        OBConversion::RegisterOptionParam("2", this);
        OBConversion::RegisterOptionParam("3", this);
      }
  };

  //Make an instance of the format class
  MOLFormat theMOLFormat;

  //*************************************
  class SDFormat : public MDLFormat
  {
    public:
      SDFormat()
      {
        OBConversion::RegisterFormat("sd",this, "chemical/x-mdl-sdfile");
        OBConversion::RegisterFormat("sdf",this, "chemical/x-mdl-sdfile");
      }

      virtual bool WriteMolecule(OBBase* pOb, OBConversion* pConv)
      {
        //The sd option ensures that a $$$$ is written at the end of the file
        pConv->AddOption("sd", OBConversion::OUTOPTIONS);
        return MDLFormat::WriteMolecule(pOb, pConv);
      }
  };

  //Make an instance of the format class
  SDFormat theSDFormat;

  // Helper for 2.3 -- is this atom a metal
  bool MDLFormat::IsMetal(OBAtom *atom)
  {
    const unsigned NMETALS = 78;
    const int metals[NMETALS] = {
    3,4,11,12,13,19,20,21,22,23,24,25,26,27,28,29,
    30,31,37,38,39,40,41,42,43,44,45,46,47,48,49,50,55,56,57,58,59,60,61,62,63,
    64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,87,88,89,90,91,
    92,93,94,95,96,97,98,99,100,101,102,103};
    return std::find(metals, metals+78, atom->GetAtomicNum())!=metals+78;
  }

  /////////////////////////////////////////////////////////////////
  bool MDLFormat::ReadMolecule(OBBase* pOb, OBConversion* pConv)
  {
    OBMol* pmol = pOb->CastAndClear<OBMol>();

    //Define some references so we can use the old parameter names
    istream &ifs = *pConv->GetInStream();
    OBMol &mol = *pmol;
    bool setDimension = false; // did we extract the 'dimensional code' from line 2?
    stringstream errorMsg;
    string clearError; // empty string to clear the warning buffer

    int i, natoms, nbonds;
    //char buffer[BUFF_SIZE];
    string comment;
    string r1, r2;
    map<OBBond*, OBStereo::BondDirection> updown;
    vector<Parity> parities;
    vector<pair<AliasData*,OBAtom*> > aliases;

    // Attempting to read past the end of the file -- don't bother
    if ( !ifs.good() || ifs.peek() == EOF )
      return false;

    std::string line;
    //
    // The Header Block
    //

    // line1: molecule name
    if (!std::getline(ifs, line)) {
      errorMsg << "WARNING: Problems reading a MDL file\n";
      errorMsg << "Cannot read title line\n";
      obErrorLog.ThrowError(__FUNCTION__, errorMsg.str() , obWarning);
      return(false);
    }

    //Do not interpret a single (usually blank) line at end of file as
    //another molecule giving an unnecessary error message.
    if ( !ifs.good() || ifs.peek() == EOF )
      return false;

    mol.SetTitle(line);

    if(pConv->IsOption("T",OBConversion::INOPTIONS))
    {
      //Read title only
      SkipObjects(0, pConv);
      return true;
    }

    if(pConv->IsOption("P",OBConversion::INOPTIONS))
    {
      //Read Title and Property lines only
      ignore(ifs, "M  END");
      ifs.ignore(100,'\n');
      ReadPropertyLines(ifs, mol);//also reads $$$$
      return true;
    }

    // line 2: IIPPPPPPPPMMDDYYHHmmddSSssssssssssEEEEEEEEEEEERRRRRR
    //
    //          0...1    I = user's initials
    //          2...9    P = program name
    //         10..19    M/D/Y,H:m = date/time
    //         20..21    d = dimensional code
    //         22..23    S = scaling factor
    //         24..33    s = scaling facter (double format 10.5)
    //         34..45    E = energy
    //         46..51    R = internal registry number
    if (!std::getline(ifs, line)) {
      errorMsg << "WARNING: Problems reading a MDL file\n";
      errorMsg << "Cannot read creator/dimension line line\n";
      obErrorLog.ThrowError(__FUNCTION__, errorMsg.str() , obWarning);
      return false;
    }

    if (line.size() > 21) {
      string dim = line.substr(20, 2);
      if (dim == "3D") {
        mol.SetDimension(3);
        setDimension = true;
      } else
      if (dim == "2D") {
        mol.SetDimension(2);
        setDimension = true;
      } else
      if (dim == "0D") {
        mol.SetDimension(0);
        setDimension = true;
      }
    }

    // line 3: comment line
    if (!std::getline(ifs, line)) {
      errorMsg << "WARNING: Problems reading a MDL file\n";
      errorMsg << "Cannot read comment line\n";
      obErrorLog.ThrowError(__FUNCTION__, errorMsg.str() , obWarning);
      return false;
    }

    if (!line.empty())
      comment = line;

    //
    // Connection Table (Ctab)
    //

    // line 1: counts line
    if (!std::getline(ifs, line)) {
      errorMsg << "WARNING: Problems reading a MDL file\n";
      errorMsg << "Cannot read atom and bond count\n";
      errorMsg << "File ended prematurely\n";
      obErrorLog.ThrowError(__FUNCTION__, errorMsg.str() , obWarning);
      return false;
    }
    if (line.size() < 6) { // error from Joe Bedell, Sigma-Aldrich
      errorMsg << "WARNING: Problems reading a MDL file\n";
      errorMsg << "Cannot read atom and bond count\n";
      errorMsg << "Expected standard 6 character atom and bond count\n";
      obErrorLog.ThrowError(__FUNCTION__, errorMsg.str() , obWarning);
      return(false);
    }

    natoms = ReadUIntField((line.substr(0, 3)).c_str());
    nbonds = ReadUIntField((line.substr(3, 3)).c_str());

    if(ReadUIntField((line.substr(6, 3)).c_str())>0)
      obErrorLog.ThrowError(__FUNCTION__,
        "WARNING: Problems reading the Count line of an MDL file\n"
        "There may be erroneous addition spaces or\n"
        "the file may contains Atom Lists, which are ignored\n",
        obWarning);

    mol.BeginModify();
    if(line.find("V3000") != string::npos) {
      // V3000
      indexmap.clear();
      if(!ReadV3000Block(ifs, mol, pConv, false))
        return false;
      //ifs.getline(buffer,BUFF_SIZE); //M END line
    } else {
      // V2000
      mol.ReserveAtoms(natoms);
      double x,y,z;
      string symbol;
      //
      // Atom Block
      //
      int massdiff, charge, stereo, isotope;
      vector<int> massDiffs, charges;
      Parity parity;
      for (i = 0; i < natoms; ++i) {
        if (!std::getline(ifs, line)) {
          errorMsg << "WARNING: Problems reading a MDL file\n";
	        errorMsg << "Not enough atoms to match atom count (" << natoms << ") in counts line\n";
	        obErrorLog.ThrowError(__FUNCTION__, errorMsg.str() , obWarning);
          return false;
	      }

        // xxxxx.xxxxyyyyy.yyyyzzzzz.zzzz aaaddcccssshhhbbbvvvHHHrrriiimmmnnneee
        //
        // 0...30   x y z = atom coordinates
        // 31..33   aaa = atom symbol
        // 34..35   dd = mass difference: -3, -2, -1, 0, 1, 2, 3, 4 ('M  ISO' lines take precedence)
        // 36..38   ccc = charge  ('M  CHG' and 'M  RAD' lines take precedence)
        // 39..41   sss = atom stereo parity (ignored)
        //          ... = query/reaction related
        // 48..50   vvv = valence (ignored unless 15, which means 0)
        massdiff = charge = 0;
        parity = NotStereo;
        if (line.size() < 34) {
	        errorMsg << "WARNING: Problems reading a MDL file\n";
	        errorMsg << "Missing data following atom specification in atom block\n";
	        obErrorLog.ThrowError(__FUNCTION__, errorMsg.str() , obWarning);
          return false;
        }

        //Need to have atom in molecule while adding data
        OBAtom* patom = mol.NewAtom();

        // coordinates
        x = atof(line.substr(0, 10).c_str());
        y = atof(line.substr(10, 10).c_str());
        z = atof(line.substr(20, 10).c_str());
        patom->SetVector(x, y, z);
        // symbol & isotope
        symbol = line.substr(31, 3);
        // cout << " atom: " << symbol << endl;
        Trim(symbol);
        if(symbol[0]!='R' || TestForAlias(symbol, patom, aliases))
        {
          isotope = 0;
          patom->SetAtomicNum(etab.GetAtomicNum(symbol, isotope));
          if (isotope != 0) // e.g. 'D' or 'T' atom symbol
            patom->SetIsotope(isotope);
        }
        // mass difference
        if (line.size() >= 35)
          massdiff = ReadIntField(line.substr(34, 2).c_str());
        massDiffs.push_back(massdiff);
        // charge
        if (line.size() >= 38)
          charge = ReadIntField(line.substr(36, 3).c_str());
        charges.push_back(charge);
        // stereo parity
        if (line.size() >= 41) {
          stereo = ReadUIntField(line.substr(39, 3).c_str());
          switch (stereo) {
            case 1:
              parity = Clockwise;
              break;
            case 2:
              parity = AntiClockwise;
              break;
            case 3:
              parity = Unknown;
              break;
            default:
              parity = NotStereo;
              break;
          }
        }
        parities.push_back(parity);
        // valence
        if (line.size() >= 50) {
          int valence = ReadIntField(line.substr(48, 3).c_str());
          if(valence!=0) // Now no H with any value
            patom->ForceNoH();
        }

//        if (!mol.AddAtom(atom))
//          return false;
//        atom.Clear();
      }

      //
      // Bond Block
      //
      stereo = 0;
      unsigned int begin, end, order, flag;
      for (i = 0;i < nbonds; ++i) {
        flag = 0;
        if (!std::getline(ifs, line)) {
          errorMsg << "WARNING: Problems reading a MDL file\n";
	        errorMsg << "Not enough bonds to match bond count (" << nbonds << ") in counts line\n";
	        obErrorLog.ThrowError(__FUNCTION__, errorMsg.str() , obWarning);
                return false;
	      }
	      begin = end = order = 0;
	      // 111222tttsssxxxrrrccc
        //
        // 111 = first atom number
        // 222 = second atom number
        // ttt = bond type (1-3, 4 = aromatic, 4-8 = query)
        // sss = bond stereo (for a double bond 3 indicates unspecified stereochem,
        //                    for a single bond 1 is Hash, 6 Wedge, 4 is unspecified)
        // ... = query/topology
        if (line.size() >= 9) {
	  begin = ReadUIntField(line.substr(0, 3).c_str());
	  end   = ReadUIntField(line.substr(3, 3).c_str());
	  order = ReadUIntField((line.substr(6, 3)).c_str());
	}
        if (begin == 0 || end == 0 || order == 0 || begin > mol.NumAtoms() || end > mol.NumAtoms()) {
	        errorMsg << "WARNING: Problems reading a MDL file\n";
	        errorMsg << "Invalid bond specification, atom numbers or bond order are wrong;\n";
	        errorMsg << "each should be in a field of three characters.\n";
	        obErrorLog.ThrowError(__FUNCTION__, errorMsg.str() , obWarning);
          return false;
	      }

        order = (order == 4) ? 5 : order;
        if (line.size() >= 12) {  //handle wedge/hash data
          stereo = ReadUIntField((line.substr(9, 3)).c_str());
          if (stereo) {
            switch (stereo) {
              case 1:
                // single bond: wedge
                flag |= OBBond::Wedge;
                break;
              case 3:
                // double bond: either cis or trans
                flag |= OBBond::CisOrTrans;
              case 4:
                // single bond: either wedge or hash (unspecified)
                flag |= OBBond::WedgeOrHash;
                break;
              case 6:
                // single bond: hash
                flag |= OBBond::Hash;
                break;
              default:
                // single bonds: not stereo
                // double bonds: use x,y,z coordinates
                break;
            }
          }
        }

        if (!mol.AddBond(begin,end,order,flag)) {
	        errorMsg << "WARNING: Problems reading a MDL file\n";
	        errorMsg << "Invalid bond specification\n";
	        obErrorLog.ThrowError(__FUNCTION__, errorMsg.str() , obWarning);
	        return false;
	      }
      }

      //
      // Properties Block
      //
      bool foundISO = false, foundCHG = false;
      while (std::getline(ifs, line)) {
        if (line.substr(0, 4) == "$$$$")
          return true;
        if (line.substr(0, 6) == "M  END")
          break;
        if (line.substr(0, 6) == "S  SKP") {
          int i = ReadUIntField((line.substr(6, line.size() - 6)).c_str());
          for(; i > 0; --i)
            if (ifs.good()) // check for EOL, suggested by Dalke
              std::getline(ifs, line);
        }

        if (line.substr(0, 3) == "A  " && line.size() > 3) { //alias
          int atomnum = ReadUIntField((line.substr(2, line.size() - 2)).c_str());
          //MDL documentation just has alias text here( x... ). A single line is assumed,
          //and the alias is ignored if the line starts with ? or * or is blank .
          std::getline(ifs, line);
          if(!line.empty() && line.at(0) != '?' && line.at(0) != '*') {
            AliasData* ad = new AliasData();
            ad->SetAlias(line);
            ad->SetOrigin(fileformatInput);
            OBAtom* at = mol.GetAtom(atomnum);
            if (at) {
              at->SetData(ad);
              //at->SetAtomicNum(0); Now leave element as found
              //The alias has now been added as a dummy atom with a AliasData object.
              //Delay the chemical interpretation until the rest of the molecule has been built
              aliases.push_back(make_pair(ad, at));
            }
          }
          continue;
        }

        if ((line.substr(0, 6) != "M  CHG") && (line.substr(0, 6) != "M  RAD") &&
            (line.substr(0, 6) != "M  ISO"))
          continue;
        unsigned int n = 0;
        if (line.size() >= 9)
          n = ReadUIntField((line.substr(6, 3)).c_str()); //entries on this line
        if (n <= 0 || n > 99 || 6+n*8 > line.size()) { //catch ill-formed line
          obErrorLog.ThrowError(__FUNCTION__, "Error in line: Invalid number following 'M  CHG', 'M  ISO' or 'M  RAD' specification (must be an integer in range 1 to 8)\n" + line, obError);
          return false;
        }
	if (n > 8) {
	  obErrorLog.ThrowError(__FUNCTION__, "Invalid line: too many items, only 8 items are allowed:\n" + line, obWarning);
	}
        int pos = 10;
        for (; n > 0; n--, pos += 8) {
          int atomnumber = ReadUIntField((line.substr(pos,3)).c_str());
          OBAtom *at;
          if (atomnumber==0 || (at=mol.GetAtom(atomnumber))==NULL) {
            obErrorLog.ThrowError(__FUNCTION__, "Error in line:\n" + line, obError);
            return false;
          }

          at = mol.GetAtom(atomnumber); //atom numbers start at 1
          int value = ReadUIntField((line.substr(pos+4,3)).c_str());
          if (line.substr(3, 3) == "RAD") {
            at->SetSpinMultiplicity(value);
            foundCHG = true;
          } else if (line.substr(3, 3) == "CHG") {
            at->SetFormalCharge(value);
            foundCHG = true;
          } else if (line.substr(3, 3) == "ISO") {
            if (value)
              at->SetIsotope(value);
            foundISO = true;
          }
        }
        // Lines setting several other properties are not implemented
      }

      // if no 'M  ISO' properties are found, use the mass differences from the atom block
      if (!foundISO)
        FOR_ATOMS_OF_MOL (a, mol) {
          int massDifference = massDiffs.at(a->GetIndex());
          if (massDifference)
            a->SetIsotope((int)(etab.GetMass(a->GetAtomicNum()) + massDifference));
        }

      // if no 'M  CHG' or 'M  RAD' properties are found, use the charges from the atom block
      if (!foundCHG)
        FOR_ATOMS_OF_MOL (a, mol) {
          charge = charges.at(a->GetIndex());
          switch (charge) {
            case 0: break;
            case 3: a->SetFormalCharge(1); break;
            case 2: a->SetFormalCharge(2); break;
            case 1: a->SetFormalCharge(3); break;
            case 5: a->SetFormalCharge(-1); break;
            case 6: a->SetFormalCharge(-2); break;
            case 7: a->SetFormalCharge(-3); break;
          }
        }
    }

    //Expand aliases
    for(vector<pair<AliasData*,OBAtom*> >::iterator iter=aliases.begin();iter!=aliases.end();++iter)
    {
      AliasData* ad = (*iter).first;
      unsigned atomnum = (*iter).second->GetIdx();
      ad->Expand(mol, atomnum); //Make chemically meaningful, if possible.
    }

    // Set up the updown map we are going to use to derive stereo info
    FOR_BONDS_OF_MOL(bond, mol) {
      OBStereo::BondDirection bd = OBStereo::NotStereo;;
      unsigned int flag = bond->GetFlags();
      if (flag & OBBond::Wedge)
        bd = OBStereo::UpBond;
      if (flag & OBBond::Hash)
        bd = OBStereo::DownBond;
      if (flag & OBBond::WedgeOrHash)
        bd = OBStereo::UnknownDir;
      if (flag & OBBond::CisOrTrans && bond->GetBondOrder()==2)
        bd = OBStereo::UnknownDir;
      if (bd != OBStereo::NotStereo)
        updown[&*bond] = bd;
    }

    mol.AssignSpinMultiplicity();
    mol.EndModify();

    if (comment.length()) {
      OBCommentData *cd = new OBCommentData;
      cd->SetData(comment);
      cd->SetOrigin(fileformatInput);
      mol.SetData(cd);
    }

    //Get property lines
    ReadPropertyLines(ifs, mol);

    if (mol.Has3D()) {
      if (!setDimension)
        mol.SetDimension(3);
      // use 3D coordinates to determine stereochemistry
      StereoFrom3D(&mol);
      if (pConv->IsOption("s", OBConversion::INOPTIONS)) { // Use the parities for tet stereo instead
        TetStereoFromParity(mol, parities, true); // True means "delete existing TetStereo first"
      }

      // For unspecified cis/trans stereos, set their Configs to unspecified
      // This should really be done in CisTransFrom3D like in CisTransFrom2D but can't change the API now :-/
      map<OBBond*, OBStereo::BondDirection>::const_iterator bd_it;
      OpenBabel::OBStereoFacade facade(&mol);
      for(bd_it=updown.begin(); bd_it!=updown.end(); ++bd_it) {
        OBBond* bond = bd_it->first;
        if (bond->GetBondOrder()!=2 || bd_it->second != OBStereo::UnknownDir)
          continue; // Only continue for those double bonds with UnknownDir
        OBCisTransStereo* ct = facade.GetCisTransStereo(bond->GetId());
        if (ct) {
          OBCisTransStereo::Config config = ct->GetConfig();
          config.specified = false;
          ct->SetConfig(config);
        }
      }
    } else
    if (mol.Has2D()) {
      if (!setDimension)
        mol.SetDimension(2);
      // use 2D coordinates + hash/wedge to determine stereochemistry
      StereoFrom2D(&mol, &updown);
      if (pConv->IsOption("s", OBConversion::INOPTIONS)) { // Use the parities for tet stereo instead
        TetStereoFromParity(mol, parities, true); // True means "delete existing TetStereo first"
      }
    } else { // 0D
      if (!setDimension)
        mol.SetDimension(0);
      // The 0D format is not considered to store stereochemistry, but
      // if you specify the "s" option, then atom parities from the
      // MOL file will be used to create tetrahedral stereochemistry
      if (pConv->IsOption("s", OBConversion::INOPTIONS))
        TetStereoFromParity(mol, parities);
      StereoFrom0D(&mol);
    }

    return true;
  }

  /////////////////////////////////////////////////////////////////
  bool MDLFormat::WriteMolecule(OBBase* pOb, OBConversion* pConv)
  {
    OBMol* pmol = dynamic_cast<OBMol*>(pOb);

    //Define some references so we can use the old parameter names
    ostream &ofs = *pConv->GetOutStream();
    OBMol &mol = *pmol;

    // Recommend using --gen2D or --gen3D
    if (mol.GetDimension()==0)
      obErrorLog.ThrowError(__FUNCTION__, "No 2D or 3D coordinates exist. Any stereochemical information will"
      " be lost. To generate 2D or 3D coordinates use --gen2D or --gen3d.", obWarning, onceOnly);

    PerceiveStereo(&mol);

    if (pConv->GetOutputIndex()==1)
      HasProperties = false;

    //
    // Header Block
    //

    string dimension("2D");
    if(mol.GetDimension()==3)
      dimension = "3D";

    if(pConv->IsOption("A"))
      AliasData::RevertToAliasForm(mol);

    // line 1: molecule name
    ofs << mol.GetTitle() <<  endl;

    // line 2: Program name, date/time, dimensions code
    ofs << " OpenBabel" << GetTimeDate() <<  dimension << endl; //line2

    // line 3: comment
    if (mol.HasData(OBGenericDataType::CommentData)) {
      OBCommentData *cd = (OBCommentData*)mol.GetData(OBGenericDataType::CommentData);
      string comment = cd->GetData();
      if(comment.size()>80)
        comment.erase(80); //truncate to 80 chars
      ofs << comment;
    }
    ofs << endl;

    //
    // Atom Block
    //

    if(pConv->IsOption("3") || mol.NumAtoms() > 999 || mol.NumBonds() > 999) {
      if (!WriteV3000(ofs, mol, pConv))
        return false;
    } else {
      //The rest of the function is the same as the original
      char buff[BUFF_SIZE];

      if (mol.NumAtoms() > 999 || mol.NumBonds() > 999) { // Three digits!
        stringstream errorMsg;
        errorMsg << "MDL Molfile conversion failed: Molecule is too large to convert." << endl;
        errorMsg << "  File format (v2000) is limited to 999 atoms or bonds." << endl;
        errorMsg << "  Molecule size: " << mol.NumAtoms() << " atoms ";
        errorMsg << "and " << mol.NumBonds() << " bonds." << endl;
        obErrorLog.ThrowError(__FUNCTION__, errorMsg.str(), obWarning);
        return false;
      }

      // Check to see if there are any untyped aromatic bonds (GetBO == 5)
      // These must be kekulized first
      FOR_BONDS_OF_MOL(b, mol) {
        if (b->GetBO() == 5) {
          mol.Kekulize();
          break;
        }
      }
      // Find which double bonds have unspecified chirality
      set<OBBond*> unspec_ctstereo = GetUnspecifiedCisTrans(mol);

      // Calculate parity of atoms and up/down bond for chiral centers (see Appendix A of ctfile.pdf)
      // For 2D, if pConv->IsOption("w", pConv->OUTOPTIONS)), then the IsWedge/IsHash bond
      // designations are used instead of calculating them. For 3D it is always calculated. For 0D never.
      map<OBBond*, OBStereo::BondDirection> updown;
      map<OBAtom*, Parity> parity;
      map<OBBond*, OBStereo::Ref> from;
      map<OBBond*, OBStereo::Ref>::const_iterator from_cit;
      GetParity(mol, parity);
      if (mol.GetDimension() == 3 || (mol.GetDimension()==2 && !pConv->IsOption("w", pConv->OUTOPTIONS)))
        TetStereoToWedgeHash(mol, updown, from);

      // The counts line:
      // aaabbblllfffcccsssxxxrrrpppiiimmmvvvvvv
      //
      // aaa = number of atoms
      // bbb = number of bonds
      // lll = number of atom lists (query)
      // ccc = chiral flag
      // ... = obsolete
      // mmm = no longer supported (default=999)
      //                         aaabbblllfffcccsssxxxrrrpppiiimmmvvvvvv
      snprintf(buff, BUFF_SIZE, "%3d%3d  0  0%3d  0  0  0  0  0999 V2000\n",
               mol.NumAtoms(), mol.NumBonds(), mol.IsChiral());
      ofs << buff;

      OBAtom *atom;
      vector<OBAtom*>::iterator i;
      int charge = 0;
      for (atom = mol.BeginAtom(i); atom; atom = mol.NextAtom(i)) {
        // convert charge
        switch (atom->GetFormalCharge()) {
          case 1: charge = 3; break;
          case 2: charge = 2; break;
          case 3: charge = 1; break;
          case -1: charge = 5; break;
          case -2: charge = 6; break;
          case -3: charge = 7; break;
          default: charge = 0; break;
        }
        Parity stereo = NotStereo;
        if (parity.find(atom) != parity.end())
          stereo = parity[atom];

        int valence = 0; //Only non-zero when RAD value would be >=4 (outside spec)
        //or an unbonded metal
        if (atom->GetSpinMultiplicity()>=4 || (IsMetal(atom) && atom->GetValence()==0))
          valence = atom->GetValence()==0 ? 15 : atom->GetValence();

        snprintf(buff, BUFF_SIZE, "%10.4f%10.4f%10.4f %-3s%2d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d",
          atom->GetX(), atom->GetY(), atom->GetZ(),
          atom->GetAtomicNum() ? etab.GetSymbol(atom->GetAtomicNum()) : "* ",
          0,charge,stereo,0,0,valence,0,0,0,0,0,0);
        ofs << buff << endl;
        }

        OBAtom *nbr;
        OBBond *bond;
        vector<OBBond*>::iterator j;
        for (atom = mol.BeginAtom(i);atom;atom = mol.NextAtom(i)) {
          for (nbr = atom->BeginNbrAtom(j);nbr;nbr = atom->NextNbrAtom(j)) {
            bond = (OBBond*) *j;
            from_cit = from.find(bond);
            // If the bond has *calculated* stereodirectionality, ensure that the start point
            // is at the 'from' atom. Otherwise, just ensure that the start atom
            // is the 'begin atom' of the bond (so that stereodirectionality that was
            // read in [rather than calculated] will be correct).
            if ( (from_cit==from.end() && atom->GetIdx()==bond->GetBeginAtomIdx()) ||
                 (from_cit!=from.end() && from_cit->second == atom->GetId()) ) {
              int stereo = 0;
              if(mol.GetDimension() == 2 && pConv->IsOption("w", pConv->OUTOPTIONS)!=NULL) {
                if (bond->IsWedge())
                  stereo = 1;
                else if (bond->IsHash())
                  stereo = 6;
                else if (bond->IsWedgeOrHash())
                  stereo = 4;
              }

              // For unspecified Cis/Trans double bonds, set the stereo to 3...
              if (unspec_ctstereo.find(bond) != unspec_ctstereo.end())
                stereo = 3;
              // For 3D (and 2D if "w" output option), set the stereo of the chiral centers.
              if (updown.find(bond) != updown.end())
                stereo = updown[bond];

              ofs << setw(3) << atom->GetIdx(); // begin atom number
              ofs << setw(3) << nbr->GetIdx(); // end atom number
              ofs << setw(3) << bond->GetBO(); // bond type
              ofs << setw(3) << stereo; // bond stereo
              ofs << "  0  0  0" << endl;
            }
          }
        }

        vector<OBAtom*> rads, isos, chgs;
        vector<OBAtom*>::iterator itr;
        for (atom = mol.BeginAtom(i);atom;atom = mol.NextAtom(i))
          {
            if(atom->GetSpinMultiplicity()>0 && atom->GetSpinMultiplicity()<4)
              rads.push_back(atom);
            if(atom->GetIsotope())
              isos.push_back(atom);
            if(atom->GetFormalCharge())
              chgs.push_back(atom);

            if(atom->HasData(AliasDataType))
            {
              AliasData* ad = static_cast<AliasData*>(atom->GetData(AliasDataType));
              if(!ad->IsExpanded()) //do nothing with an expanded alias
                ofs << "A  " << setw(3) << right << atom->GetIdx() << '\n' << ad->GetAlias() << endl;
            }
            //Atoms with no AliasData, but 0 atomicnum and atomclass==n are given an alias Rn
            else if(atom->GetAtomicNum()==0)
            {
              OBAtomClassData* pac = static_cast<OBAtomClassData*>(mol.GetData("Atom Class"));
              if(pac && pac->HasClass(atom->GetIdx()))
                ofs << "A  " << setw(3) << right << atom->GetIdx() << '\n'
                    << 'R' << pac->GetClass(atom->GetIdx()) << endl;
            }
          }
        if (rads.size()) {
	  int counter = 0;
	  for(itr=rads.begin();itr!=rads.end();++itr, counter++) {
	    if (counter % 8 == 0) {
	      if (counter > 0) ofs << endl;
	      ofs << "M  RAD" << setw(3) << min(static_cast<unsigned long int>(rads.size() - counter), static_cast<unsigned long int>(8));
	    }
	    ofs << setw(4) << (*itr)->GetIdx() << setw(4) << (*itr)->GetSpinMultiplicity();
	  }
	  ofs << endl;
	}
        if(isos.size()) {
	  int counter = 0;
	  for(itr=isos.begin();itr!=isos.end();++itr, counter++) {
	    if (counter % 8 == 0) {
	      if (counter > 0) ofs << endl;
	      ofs << "M  ISO" << setw(3) << min(static_cast<unsigned long int>(isos.size() - counter), static_cast<unsigned long int>(8));
	    }
	    ofs << setw(4) << (*itr)->GetIdx() << setw(4) << (*itr)->GetIsotope();
	  }
	  ofs << endl;
	}
        if(chgs.size()) {
	  int counter = 0;
	  for (itr=chgs.begin(); itr != chgs.end(); ++itr, counter++) {
	    if (counter % 8 == 0) {
	      if (counter > 0) ofs << endl;
	      ofs << "M  CHG" << setw(3) << min(static_cast<unsigned long int>(chgs.size() - counter), static_cast<unsigned long int>(8));
	    }
	    ofs << setw(4) << (*itr)->GetIdx() << setw(4) << (*itr)->GetFormalCharge();
	  }
	  ofs << endl;
	}
      }

    ofs << "M  END" << endl;

    //For SD files only, write properties unless option m
    if(pConv->IsOption("sd") && !pConv->IsOption("m"))
      {
        vector<OBGenericData*>::iterator k;
        vector<OBGenericData*> vdata = mol.GetData();
        for (k = vdata.begin();k != vdata.end();k++)
          {
            if ((*k)->GetDataType() == OBGenericDataType::PairData
                && (*k)->GetOrigin()!=local) //internal OBPairData is not written
              {
                HasProperties = true;
                //Since partial charges are not output
                //in this format, don't need the annotation
                if((*k)->GetAttribute()!="PartialCharges")
                {
                  ofs << ">  <" << (*k)->GetAttribute() << ">" << endl;
                  ofs << ((OBPairData*)(*k))->GetValue() << endl << endl;
                }
              }
          }
      }

    //Unless option no$$$$ is set, $$$$ is always written between molecules and
    //at the end any if properties have been output in any molecule,
    //or if the sd option is set.
    if(!pConv->IsOption("no$$$$"))
      if(!pConv->IsLast()  || HasProperties  || pConv->IsOption("sd"))
        ofs << "$$$$" << endl;

    return(true);
  }


  //////////////////////////////////////////////////////
  bool MDLFormat::ReadV3000Block(istream& ifs, OBMol& mol, OBConversion* pConv,bool DoMany)
  {
    bool ret = true;
    do
      {
        if(!ReadV3000Line(ifs,vs)) return false;
        if(vs[1]=="END") return true;
        if(vs[2]=="LINKNODE"){continue;} //not implemented
        if(vs[2]!="BEGIN") return false;

        if(vs[3]=="CTAB")
          {
            if(!ReadV3000Line(ifs,vs) || vs[2]!="COUNTS") return false;
            int natoms = ReadUIntField(vs[3].c_str());
            //int nbonds = ReadUIntField(vs[4].c_str());
            //int chiral = ReadUIntField(vs[7].c_str());
            //number of s groups, number of 3D contraints, chiral flag and regno not yet implemented
            mol.ReserveAtoms(natoms);

            ReadV3000Block(ifs,mol,pConv,true);//go for contained blocks
            if(vs[2]!="END" && vs[3]!="CTAB") return false;
            ret= true;
          }
        else if(vs[3]=="ATOM")
          ret = ReadAtomBlock(ifs,mol,pConv);
        else if(vs[3]=="BOND")
          ret = ReadBondBlock(ifs,mol,pConv);
        //else if(vs[3]=="COLLECTION")
        //  ret = ReadCollectionBlock(ifs,mol,pConv);
        else if(vs[3]=="RGROUP")
          ret = ReadRGroupBlock(ifs,mol,pConv);
        else
          ret =ReadUnimplementedBlock(ifs,mol,pConv,vs[3]);
        /*
          else if(vs[3]=="3D")
          //not currently implemented
          else if(vs[3]=="SGROUP")
          //not currently implemented
        */
      }while(ret && ifs.good());
    //  if(is3D){mol.SetDimension(3);cout<<"SetDim to 3"<<endl;}
    //  else if(is2D){mol.SetDimension(2);cout<<"SetDim to 2"<<endl;}
    return true;
  }

  //////////////////////////////////////////////////////
  bool MDLFormat::ReadV3000Line(istream& ifs, vector<string>& vs)
  {
    char buffer[BUFF_SIZE];
    if(!ifs.getline(buffer,BUFF_SIZE)) return false;
    tokenize(vs,buffer," \t\n\r");
    if (vs.size() < 2) return false; // timvdm 18/06/2008
    if(vs[0]!="M" || (vs[1]!="V30" && vs[1]!="END")) return false;

    if(buffer[strlen(buffer)-1] == '-') //continuation char
      {
        //Read continuation line iteratively and add parsed tokens (without M V30) to vs
        vector<string> vsx;
        if(!ReadV3000Line(ifs,vsx)) return false;
        vs.insert(vs.end(),vsx.begin()+3,vsx.end());
      }
    return true;
  }

  //////////////////////////////////////////////////////
  bool MDLFormat::ReadAtomBlock(istream& ifs,OBMol& mol, OBConversion* pConv)
  {
    OBAtom atom;
    bool chiralWatch=false;
    int obindex;
    for(obindex=1;;obindex++)
      {
        if(!ReadV3000Line(ifs,vs)) return false;
        if(vs[2]=="END") break;

        indexmap[ReadUIntField(vs[2].c_str())] = obindex;
        atom.SetVector(atof(vs[4].c_str()), atof(vs[5].c_str()), atof(vs[6].c_str()));
        //      if(abs(atof(vs[6].c_str()))>0)is3D=true;
        //      if(abs(atof(vs[4].c_str()))>0)is2D=true;
        //      if(abs(atof(vs[5].c_str()))>0)is2D=true;
        char type[5];
        strncpy(type,vs[3].c_str(),5);
        type[4] = '\0'; // ensure it's always null-terminated
        if(!strcmp(type, "R#"))
          {
          obErrorLog.ThrowError(__FUNCTION__,
            "A molecule contains an R group which are not currently implemented"
            , obWarning, onceOnly);
          atom.SetAtomicNum(0);
          }
        else
          {
          int iso=0;
          atom.SetAtomicNum(etab.GetAtomicNum(type,iso));
          if(iso)
            atom.SetIsotope(iso);
          atom.SetType(type); //takes a char not a const char!
          //mapping vs[7] not implemented

          //Atom properties
          vector<string>::iterator itr;
          for(itr=vs.begin()+8;itr!=vs.end();itr++)
            {
              string::size_type pos = (*itr).find('=');
              if (pos==string::npos) return false;
              int val = ReadIntField((*itr).substr(pos+1).c_str());

              if((*itr).substr(0,pos)=="CHG")
                {
                  atom.SetFormalCharge(val);
                }
              else if((*itr).substr(0,pos)=="RAD")
                {
                  atom.SetSpinMultiplicity(val);
                }
              else if((*itr).substr(0,pos)=="CFG")
                {
                  //Stereo configuration: 0 none; 1 odd parity; 2 even parity; (3 either parity)
                  //Reversed 12Aug05 as advised by Nick England
                 /* @todo
                  if(val==2) atom.SetAntiClockwiseStereo();
                  else if(val==1) atom.SetClockwiseStereo();
                  else if(val==3) atom.SetChiral();
                  chiralWatch=true;
                  */
                }
              else if((*itr).substr(0,pos)=="MASS")
                {
                  if(val) atom.SetIsotope(val);
                }
              else if((*itr).substr(0,pos)=="VAL")
                {
                  //@todo Abnormal valence: 0 normal;-1 zero
                }
              //Several query properties unimplemented
              //Unknown properties ignored
            }
          }
        if(!mol.AddAtom(atom)) return false;
        /*
        if(chiralWatch)
          _mapcd[mol.GetAtom(mol.NumAtoms())]= new OBChiralData; // fill the map with chrial data for each chiral atom
        */
        atom.Clear();
      }
    return true;
  }

  //////////////////////////////////////////////////////
  bool MDLFormat::ReadBondBlock(istream& ifs,OBMol& mol, OBConversion* pConv)
  {
    for(;;)
      {
        if(!ReadV3000Line(ifs,vs)) return false;
        if(vs[2]=="END") break;

        unsigned flag=0;

        int order = ReadUIntField(vs[3].c_str());
        if(order==4) order=5;

        int obstart = indexmap[ReadUIntField(vs[4].c_str())];
        int obend = indexmap[ReadUIntField(vs[5].c_str())];

        vector<string>::iterator itr;
        for(itr=vs.begin()+6;itr!=vs.end();itr++)
          {
            string::size_type pos = (*itr).find('=');
            if (pos==string::npos) return false;
            int val = ReadUIntField((*itr).substr(pos+1).c_str());

            if((*itr).substr(0,pos)=="CFG")
              {
                //@todo Bond Configuration 2 or 3D??
                if (val == 1)
                  {
                    flag |= OB_WEDGE_BOND;
                  }
                else if (val == 3)
                  {
                    flag |= OB_HASH_BOND;
                  }
              }
          }
        if (!mol.AddBond(obstart,obend,order,flag)) return false;

        /*
        // after adding a bond to atom "obstart"
        // search to see if atom is bonded to a chiral atom
        map<OBAtom*,OBChiralData*>::iterator ChiralSearch;
        ChiralSearch = _mapcd.find(mol.GetAtom(obstart));
        if (ChiralSearch!=_mapcd.end())
          {
            (ChiralSearch->second)->AddAtomRef(obend, input);
          }
        // after adding a bond to atom "obend"
        // search to see if atom is bonded to a chiral atom
        ChiralSearch = _mapcd.find(mol.GetAtom(obend));
        if (ChiralSearch!=_mapcd.end())
          {
            (ChiralSearch->second)->AddAtomRef(obstart, input);
          }
        */
      }
    return true;
  }

////////////////////////////////////////////////////////////
  bool MDLFormat::ReadUnimplementedBlock(istream& ifs,OBMol& mol, OBConversion* pConv, string& blockname)
  {
    //Not currently implemented
    obErrorLog.ThrowError(__FUNCTION__,
      blockname + " blocks are not currently implemented and their contents are ignored.", obWarning, onceOnly);
    for(;;)
    {
      if(!ReadV3000Line(ifs,vs))
        return false;
      if(vs[2]=="END")
        break;
    }
    return true;
  }

////////////////////////////////////////////////////////////
  bool MDLFormat::ReadRGroupBlock(istream& ifs,OBMol& mol, OBConversion* pConv)
  {
    //Not currently implemented
    obErrorLog.ThrowError(__FUNCTION__,
      "RGROUP and RLOGIC blocks are not currently implemented and their contents are ignored.",
      obWarning, onceOnly);
    for(;;)
    {
      if(!ReadV3000Line(ifs,vs))
        return false;
      if(vs[2]=="END" && vs[3]=="RGROUP")
        break;
    }
    return true;
  }

  //////////////////////////////////////////////////////////
  bool MDLFormat::WriteV3000(ostream& ofs,OBMol& mol, OBConversion* pConv)
  {
    // Check to see if there are any untyped aromatic bonds (GetBO == 5)
    // These must be kekulized first
    FOR_BONDS_OF_MOL(b, mol)
      {
        if (b->GetBO() == 5)
          {
            mol.Kekulize();
            break;
          }
      }


    ofs << "  0  0  0     0  0            999 V3000" << endl; //line 4
    ofs << "M  V30 BEGIN CTAB" <<endl;
    ofs << "M  V30 COUNTS " << mol.NumAtoms() << " " << mol.NumBonds()
        << " 0 0 " << mol.IsChiral() << endl;

    ofs << "M  V30 BEGIN ATOM" <<endl;
    OBAtom *atom;
    int index=1;
    vector<OBAtom*>::iterator i;
    for (atom = mol.BeginAtom(i);atom;atom = mol.NextAtom(i))
      {
        ofs     << "M  V30 "
                << index++ << " "
                << etab.GetSymbol(atom->GetAtomicNum()) << " "
                << atom->GetX() << " "
                << atom->GetY() << " "
                << atom->GetZ()
                << " 0";
        if(atom->GetFormalCharge()!=0)
          ofs << " CHG=" << atom->GetFormalCharge();
        if(atom->GetSpinMultiplicity()!=0)
          ofs << " RAD=" << atom->GetSpinMultiplicity();
        /*
        if(atom->IsChiral())
          {
            // MOLV3000 uses 1234 unless an H then 123H

            OBChiralData* cd=(OBChiralData*)atom->GetData(OBGenericDataType::ChiralData);
            if(!cd){ //if no Chiral Data Set, need to make one!
              cd=new OBChiralData;
              atom->SetData(cd);
            }
            if (atom->GetHvyValence()==3)
              {
                OBAtom *nbr;
                int Hid = (mol.NumAtoms()+1) ;// max Atom ID +1
                vector<unsigned int> nbr_atms;
                vector<OBBond*>::iterator i;
                for (nbr = atom->BeginNbrAtom(i);nbr;nbr = atom->NextNbrAtom(i))
                  {
                    if (nbr->IsHydrogen()){Hid=nbr->GetIdx();continue;}
                    nbr_atms.push_back(nbr->GetIdx());
                  }
                sort(nbr_atms.begin(),nbr_atms.end());
                nbr_atms.push_back(Hid);
                cd->SetAtom4Refs(nbr_atms,output);
              }
            else if (atom->GetHvyValence()==4)
              {
                vector<unsigned int> nbr_atms;
                int n;
                for(n=1;n<5;n++)nbr_atms.push_back(n);
                cd->SetAtom4Refs(nbr_atms,output);
              }
            double vol=0;
            if (mol.HasNonZeroCoords())
              {
                vol=CalcSignedVolume(mol,atom);
                if (vol > 0.0)atom->SetClockwiseStereo();
                else if(vol < 0.0)atom->SetAntiClockwiseStereo();
                CorrectChirality(mol,atom,calcvolume,output);
              }
            else {
              CorrectChirality(mol,atom); // will set the stereochem based on input/output atom4refs
            }

            int cfg=3; // if we don't know, then it's unspecified
            if(atom->IsClockwise())cfg=1;
            else if(atom->IsAntiClockwise())cfg=2;

            ofs << " CFG=" << cfg;
          }
        */
        if(atom->GetIsotope()!=0)
          ofs << " MASS=" << atom->GetIsotope();
        ofs << endl;
      }
    ofs << "M  V30 END ATOM" <<endl;

    ofs << "M  V30 BEGIN BOND" <<endl;
    //so the bonds come out sorted
    index=1;
    OBAtom *nbr;
    OBBond *bond;
    vector<OBBond*>::iterator j;
    for (atom = mol.BeginAtom(i);atom;atom = mol.NextAtom(i))
      {
        for (nbr = atom->BeginNbrAtom(j);nbr;nbr = atom->NextNbrAtom(j))
          {
            if (atom->GetIdx() < nbr->GetIdx())
              {
                bond = (OBBond*) *j;
                ofs << "M  V30 "
                    << index++ << " "
                    << bond->GetBO() << " "
                    << bond->GetBeginAtomIdx() << " "
                    << bond->GetEndAtomIdx();
                //@todo do the following stereo chemistry properly
                int cfg=0;
                if(bond->IsWedge()) cfg=1;
                if(bond->IsHash()) cfg=6;
                if(bond->IsWedgeOrHash()) cfg=4;
                if(cfg) ofs << " CFG=" << cfg;
                ofs << endl;
              }
          }
      }
    ofs << "M  V30 END BOND" <<endl;
    ofs << "M  V30 END CTAB" <<endl;
    return true;
  }

  string MDLFormat::GetTimeDate()
  {
    char td[11];
    //returns MMDDYYHHmm
    struct tm* ts;
    time_t long_time;
    time( &long_time );
    ts = localtime( &long_time );
    snprintf(td, 11, "%02d%02d%02d%02d%02d", ts->tm_mon+1, ts->tm_mday,
             ((ts->tm_year>=100)? ts->tm_year-100 : ts->tm_year),
             ts->tm_hour, ts->tm_min);
    return string(td);
  }

  void MDLFormat::GetParity(OBMol& mol, map<OBAtom*, MDLFormat::Parity> &parity)
  {
    // This loop sets the atom parity for each tet center
    std::vector<OBGenericData*> vdata = mol.GetAllData(OBGenericDataType::StereoData);
    for (std::vector<OBGenericData*>::iterator data = vdata.begin(); data != vdata.end(); ++data)
      if (((OBStereoBase*)*data)->GetType() == OBStereo::Tetrahedral) {
        OBTetrahedralStereo *ts = dynamic_cast<OBTetrahedralStereo*>(*data);

        OBTetrahedralStereo::Config cfg = ts->GetConfig();

        Parity atomparity = Unknown;
        if (cfg.specified && cfg.winding != OBStereo::UnknownWinding) {
          // If, when looking towards the maxref, the remaining refs increase in number
          // clockwise, parity is 1 (Parity::Clockwise). Note that Implicit Refs and Hydrogens
          // should be treated considered the maxref if present.
          OBStereo::Refs refs = cfg.refs;

          unsigned long maxref = OBStereo::NoRef;
          // Search for an explicit Hydrogen in the cfg refs...
          if (cfg.from != OBStereo::ImplicitRef && mol.GetAtomById(cfg.from)->IsHydrogen())
            maxref = cfg.from;
          else
            for (OBStereo::RefIter ref_it = refs.begin(); ref_it != refs.end(); ++ref_it)
              if ((*ref_it) != OBStereo::ImplicitRef && mol.GetAtomById(*ref_it)->IsHydrogen())
                maxref = *ref_it;
          // ...otherwise, find the maximum ref (note that ImplicitRef will be max if present)
          if (maxref == OBStereo::NoRef)
            maxref = std::max(*(std::max_element(refs.begin(), refs.end())), cfg.from);

          // Get a new cfg and refs looking towards the maxref
          cfg = ts->GetConfig(maxref, OBStereo::Clockwise, OBStereo::ViewTowards);
          int inversions = OBStereo::NumInversions(cfg.refs);

          // If they were in increasing order, inversions would be 0 or some even value
          if (inversions % 2 == 0)
            atomparity = Clockwise;
          else
            atomparity = AntiClockwise;
        }
        parity[mol.GetAtomById(cfg.center)] = atomparity;
      }
  }

  void MDLFormat::TetStereoFromParity(OBMol& mol, vector<MDLFormat::Parity> &parity, bool deleteExisting)
  {
    if (deleteExisting) { // Remove any existing tet stereo
      std::vector<OBGenericData*> vdata = mol.GetAllData(OBGenericDataType::StereoData);
      for (std::vector<OBGenericData*>::iterator data = vdata.begin(); data != vdata.end(); ++data)
        if (((OBStereoBase*)*data)->GetType() == OBStereo::Tetrahedral)
          mol.DeleteData(*data);
    }

    for (unsigned long i=0;i<parity.size();i++) {
      if (parity[i] == NotStereo)
        continue;

      OBStereo::Refs refs;
      unsigned long towards = OBStereo::ImplicitRef;
      FOR_NBORS_OF_ATOM(nbr, mol.GetAtomById(i)) {
        if (!nbr->IsHydrogen())
          refs.push_back(nbr->GetId());
        else
          towards = nbr->GetId(); // Look towards the H
      }

      sort(refs.begin(), refs.end());
      if (refs.size() == 4) { // No implicit ref or H present
        towards = refs.back();
        refs.pop_back();
      }

      OBStereo::Winding winding = OBStereo::Clockwise;
      if (parity[i] == AntiClockwise)
        winding = OBStereo::AntiClockwise;
      OBTetrahedralStereo::Config cfg = OBTetrahedralStereo::Config(i, towards, refs,
                                                                    winding, OBStereo::ViewTowards);
      if (parity[i] == Unknown)
        cfg.specified = false;

      OBTetrahedralStereo *th = new OBTetrahedralStereo(&mol);
      th->SetConfig(cfg);
      mol.SetData(th);
    }
  }

  int MDLFormat::ReadIntField(const char *s)
  {
    char *end;
    if (s == NULL) return 0;
    int n = strtol(s, &end, 10);
    if (*end != '\0' && *end != ' ') return 0;
    return n;
  }

  unsigned int MDLFormat::ReadUIntField(const char *s)
  {
    char *end;
    if (s == NULL) return 0;
    int n = strtoul(s, &end, 10);
    if (*end != '\0' && *end != ' ') return 0;
    return n;
  }

  void MDLFormat::ReadPropertyLines(istream& ifs, OBMol& mol)
  {
    string line;
    while (std::getline(ifs, line)) {
      if (line.find("<") != string::npos) {
        size_t lt = line.find("<")+1;
        size_t rt = line.find_last_of(">");
        string attr = line.substr(lt, rt - lt);

        // sometimes we can hit more data than BUFF_SIZE, so we'll use a std::string
        string buff;
        while (std::getline(ifs, line)) {
          Trim(line);
          if (line.size()) {
            buff.append(line);
            buff += "\n";
          } else
            break;
        }
        Trim(buff);

        OBPairData *dp = new OBPairData;
        dp->SetAttribute(attr);
        dp->SetValue(buff);
        dp->SetOrigin(fileformatInput);
        mol.SetData(dp);

        if(!strcasecmp(attr.c_str(),"NAME") && *mol.GetTitle()=='\0')
          mol.SetTitle(buff);
      }
      if (line.substr(0, 4) ==  "$$$$")
        break;
      if (line.substr(0, 4) == "$MOL")
        break;
    }
  }

  bool MDLFormat::TestForAlias(const string& symbol, OBAtom* at, vector<pair<AliasData*,OBAtom*> >& aliases)
  {
  /*If symbol is R R' R'' R� R�� or Rn Rnn where n is an digit
    the atom is added to the alias list and the atomic number set to zero. Returns false.
    Otherwise, e.g Rh or Ru, returns true.
  */
    if(symbol.size()==1 || isdigit(symbol[1]) || symbol[1]=='\'' || symbol[1]=='�')
    {
      AliasData* ad = new AliasData();
      ad->SetAlias(symbol);
      ad->SetOrigin(fileformatInput);
      at->SetData(ad);
      at->SetAtomicNum(0);
      //The alias has now been added as a dummy atom with a AliasData object.
      //Delay the chemical interpretation until the rest of the molecule has been built
      aliases.push_back(make_pair(ad, at));
      return false;
    }
    return true;
  }

}//namespace
