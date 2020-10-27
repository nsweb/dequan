/*
	The MIT License (MIT)
	Copyright (c) 2020 nsweb
	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:
	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

/*
	Please define FCHECK_IMPLEMENTATION before including this file in one C / C++ file to create the implementation.
	C++ 11
*/

#ifdef FCHECK_USE_STDVECTOR
#include <vector>
#endif

/**/
namespace fcheck
{
#ifdef FCHECK_USE_STDVECTOR
	template<class T>
	using Array = std::vector<T>;
#endif

	using VarId = int;

	struct Domain
	{
		Domain() = default;

		Array<int> values;
	};

	struct SavedDomain
	{
		SavedDomain() = default;

		VarId var_id;
		Array<int> values;
	};

	struct SavedDomainStep
	{
		SavedDomainStep() = default;

		int step = 0;
		Array<SavedDomain> domains;
	};

	struct InstVar
	{
		InstVar() = default;

		static const int UNASSIGNED = -INT_MAX;

		int value = InstVar::UNASSIGNED;
	};

	struct Var
	{
		Var() = default;

		static const VarId INVALID = -1;

		VarId var_id = Var::INVALID;
		const char* name = nullptr;
		Array<int> linked_constraints;
		Domain domain;
	};

	struct Constraint
	{
		Constraint() = default;

		bool IsEnforceIf() const { return vid_if != Var::INVALID; }
		/** Returns true if respected, false otherwise - passed values must be properly instanciated */
		bool Evaluate(int vid1_val, int vid2_val) const;

		enum class Op : int
		{
			Equal = 0,	// ==
			NotEqual,	// !=
			SupEqual,	// >=
			Sup,		// >
			InfEqual,	// <=
			Inf			// <
		};

		VarId vid1 = Var::INVALID;
		Op op = Op::Equal;
		VarId vid2 = Var::INVALID;
		int const_mul2 = 1;
		int const_add2 = 0;
		VarId vid_if = Var::INVALID;
		int ifnot = 0;
	};

	class Assignment
	{
	public:
		Assignment();
		void Reset(const Array<Var>& vars);
		bool IsComplete();
		int GetInstVarValue(VarId vid) const;
		const Domain& GetCurrentDomain(VarId vid) const;
		VarId NextUnassignedVar();
		void AssignVar(const Var& var, int domain_value_idx);
		void UnAssignVar(const Var& var);
		bool ValidateConstraints(const Array<Constraint>& all_constraints, const Var& var, int val) const;
		bool AplyArcConsistency(const Constraint& con, const Var& var);
		SavedDomain& FindOrAddSavedDomain(VarId vid, const Domain& dom);
		void RestoreSavedDomainStep();

		int assigned_var_count = 0;
		//int unassigned_idx = 0;
		Array<InstVar> inst_vars;
		Array<Domain> current_domains;			// domains, changed on the go by the forward-checking algo
		Array<SavedDomainStep> saved_domains;	// backup_domains to restore after a forward-checking step
	};

	class CSP
	{
	public:
		CSP() = default;

		VarId AddIntVar(const char* name_id, const Domain& domain);
		VarId AddBoolVar(const char* name_id);
		void AddConstraint(VarId vid1, Constraint::Op op, VarId vid2, int const_mul2 = 1, int const_add2 = 0);
		void AddConstraintIf(VarId vid1, Constraint::Op op, VarId vid2, int const_mul2, int const_add2, VarId vid_if);
		void AddConstraintIfNot(VarId vid1, Constraint::Op op, VarId vid2, int const_mul2, int const_add2, VarId vid_ifnot);
		void PushConstraint(const Constraint& con);

		bool ForwardCheckingStep(Assignment& a) const;
		const Var& GetVar(VarId vid) const;
		const Domain& GetVarDomain(VarId vid) const;

		// Static parameters, unaffected by searching algo
		Array<Var> vars;
		Array<Constraint> constraints;
		Var null_var;
		Domain null_domain;
	};

}; /*namespace fcheck*/

#ifdef FCHECK_IMPLEMENTATION

namespace fcheck
{
	Assignment::Assignment() {}

	void Assignment::Reset(const Array<Var>& vars)
	{
		assigned_var_count = 0;
		//unassigned_idx = 0;

		inst_vars.clear();
		inst_vars.resize(vars.size());

		current_domains.resize(vars.size());
		for (int i = 0; i < vars.size(); i++)
		{
			current_domains[i] = vars[i].domain;
		}

		saved_domains.clear();
	}

	bool Assignment::IsComplete() { return assigned_var_count == inst_vars.size(); }

	const Domain& Assignment::GetCurrentDomain(VarId vid) const
	{
		return current_domains[vid];
	}

	int Assignment::GetInstVarValue(VarId vid) const
	{
		return inst_vars[vid].value;
	}

	VarId Assignment::NextUnassignedVar()
	{
		for (int idx = 0/*unassigned_idx*/; idx < inst_vars.size(); idx++)
		{
			if (inst_vars[idx].value == InstVar::UNASSIGNED)
			{
				//unassigned_idx = idx + 1;
				return (VarId)idx;
			}
		}
		return (VarId)inst_vars.size();
	}

	void Assignment::AssignVar(const Var& var, int dom_value_idx)
	{
		inst_vars[var.var_id].value = var.domain.values[dom_value_idx];
		assigned_var_count++;
	}

	void Assignment::UnAssignVar(const Var& var)
	{
		inst_vars[var.var_id].value = InstVar::UNASSIGNED;
		assigned_var_count--;
	}

	void Assignment::RestoreSavedDomainStep()
	{
		SavedDomainStep& domain_step = saved_domains.back();
		for (int d_idx = 0; d_idx < domain_step.domains.size(); d_idx++)
		{
			VarId vid = domain_step.domains[d_idx].var_id;
			current_domains[vid].values = domain_step.domains[d_idx].values;
		}
	}

	bool Assignment::ValidateConstraints(const Array<Constraint>& all_constraints, const Var& var, int val) const
	{
		for (int c_idx = 0; c_idx < var.linked_constraints.size(); c_idx++)
		{
			const Constraint& con = all_constraints[var.linked_constraints[c_idx]];
			if (con.IsEnforceIf())
			{
				int vid_if_val = (var.var_id == con.vid_if ? val : inst_vars[con.vid_if].value);
				if (vid_if_val == InstVar::UNASSIGNED)
					continue;	// if val is unassigned, we can't enforce the constraint yet, ignore it

				if (!!con.ifnot == !!vid_if_val)
					continue;	// conditions are not met, we should not enforce if
			}

			int vid1_val = (var.var_id == con.vid1 ? val : inst_vars[con.vid1].value);
			int vid2_val = (var.var_id == con.vid2 ? val : inst_vars[con.vid2].value);

			// Can we evaluate the constraint yet ?
			if (vid1_val != InstVar::UNASSIGNED &&
				vid2_val != InstVar::UNASSIGNED)
			{
				// Is constraint respected ?
				if (!con.Evaluate(vid1_val, vid2_val))
				{
					return false;
				}
			}
		}

		return true;
	}

	bool Constraint::Evaluate(int vid1_val, int vid2_val) const
	{
		// constraint enforcement should be checked beforce calling this function

		switch (op)
		{
		case Op::Equal:
			if (vid1_val == vid2_val * const_mul2 + const_add2)
				return true;
			break;
		case Op::NotEqual:
			if (vid1_val != vid2_val * const_mul2 + const_add2)
				return true;
			break;
		case Op::SupEqual:
			if (vid1_val >= vid2_val * const_mul2 + const_add2)
				return true;
			break;
		case Op::Sup:
			if (vid1_val > vid2_val * const_mul2 + const_add2)
				return true;
			break;
		case Op::InfEqual:
			if (vid1_val <= vid2_val * const_mul2 + const_add2)
				return true;
			break;
		case Op::Inf:
			if (vid1_val < vid2_val * const_mul2 + const_add2)
				return true;
			break;
		};

		return false;
	}

	bool Assignment::AplyArcConsistency(const Constraint& con, const Var& var)
	{
		int vid1_val = inst_vars[con.vid1].value;
		int vid2_val = inst_vars[con.vid2].value;

		if (con.IsEnforceIf())
		{
			int vid_if_val = inst_vars[con.vid_if].value;
			if (vid_if_val == InstVar::UNASSIGNED)
			{
				if (vid1_val != InstVar::UNASSIGNED && vid2_val != InstVar::UNASSIGNED)
				{
					// check for if condition that does not violate constraint
					bool is_con_ok = con.Evaluate(vid1_val, vid2_val);
					if (!is_con_ok)	// is constraint is ok, state of if-condition does not matter
					{
						Domain& dom = current_domains[con.vid_if];
						SavedDomain& sav_dom = FindOrAddSavedDomain(con.vid_if, dom);

						for (int d_idx = 0; d_idx < dom.values.size(); )
						{
							int val = dom.values[d_idx];
							if (!!con.ifnot == !!val)
							{
								// only choose values that do not enforce condition
								d_idx++;
							}
							else
							{
								dom.values.erase(dom.values.begin() + d_idx);
							}
						}
						if (dom.values.empty())
						{
							// Domain wipe out
							return false;
						}
					}
				}
				else
				{
					// otherwise the constraint is undertermined, just exit
					return true;
				}
			}
			else
			{
				if (!!con.ifnot == !!vid_if_val)
				{
					return true;	// conditions are not met, we should not enforce if
				}
			}
		}
		else
		{
			// there are only two vars, and one has just been assigned
			if (vid1_val == InstVar::UNASSIGNED)
			{
				Domain& dom = current_domains[con.vid1];
				SavedDomain& sav_dom = FindOrAddSavedDomain(con.vid1, dom);

				for (int d_idx = 0; d_idx < dom.values.size(); )
				{
					int val = dom.values[d_idx];
					if (con.Evaluate(val, vid2_val))
					{
						// only choose values that do not enforce condition
						d_idx++;
					}
					else
					{
						dom.values.erase(dom.values.begin() + d_idx);
					}
				}
				if (dom.values.empty())
				{
					// Domain wipe out
					return false;
				}
			}
			else if (vid2_val == InstVar::UNASSIGNED)
			{
				Domain& dom = current_domains[con.vid2];
				SavedDomain& sav_dom = FindOrAddSavedDomain(con.vid2, dom);

				for (int d_idx = 0; d_idx < dom.values.size(); )
				{
					int val = dom.values[d_idx];
					if (con.Evaluate(vid1_val, val))
					{
						// only choose values that do not enforce condition
						d_idx++;
					}
					else
					{
						dom.values.erase(dom.values.begin() + d_idx);
					}
				}
				if (dom.values.empty())
				{
					// Domain wipe out
					return false;
				}
			}
		}

		return true;
	}

	SavedDomain& Assignment::FindOrAddSavedDomain(VarId vid, const Domain& dom)
	{
		SavedDomainStep& domain_step = saved_domains.back();
		for (int d_idx = 0; d_idx < domain_step.domains.size(); d_idx++)
		{
			if (domain_step.domains[d_idx].var_id == vid)
				return domain_step.domains[d_idx];
		}
		domain_step.domains.push_back(SavedDomain{ vid, dom.values });
		return domain_step.domains.back();
	}

	VarId CSP::AddIntVar(const char* name_id, const Domain& domain)
	{
		Var new_var = { (VarId)vars.size(), name_id, {}, domain };
		vars.push_back(new_var);

		return new_var.var_id;
	}
	VarId CSP::AddBoolVar(const char* name_id)
	{
		Var new_var = { (VarId)vars.size(), name_id, {}, {{0, 1}} };
		vars.push_back(new_var);

		return new_var.var_id;
	}
	void CSP::AddConstraint(VarId vid1, Constraint::Op op, VarId vid2, int const_mul2, int const_add2)
	{
		Constraint new_constraint = { vid1, op, vid2, const_mul2, const_add2, Var::INVALID, 0 };
		PushConstraint(new_constraint);
	}
	void CSP::AddConstraintIf(VarId vid1, Constraint::Op op, VarId vid2, int const_mul2, int const_add2, VarId vid_if)
	{
		Constraint new_constraint = { vid1, op, vid2, const_mul2, const_add2, vid_if, 0 };
		PushConstraint(new_constraint);
	}
	void CSP::AddConstraintIfNot(VarId vid1, Constraint::Op op, VarId vid2, int const_mul2, int const_add2, VarId vid_ifnot)
	{
		Constraint new_constraint = { vid1, op, vid2, const_mul2, const_add2, vid_ifnot, 1 };
		PushConstraint(new_constraint);
	}
	void CSP::PushConstraint(const Constraint& con)
	{
		int cid = (int)constraints.size();
		constraints.push_back(con);

		Var& var1 = vars[con.vid1];
		var1.linked_constraints.push_back(cid);
		Var& var2 = vars[con.vid2];
		var2.linked_constraints.push_back(cid);

		if (con.vid_if != Var::INVALID)
		{
			Var& var_if = vars[con.vid_if];
			var_if.linked_constraints.push_back(cid);
		}
	}

	const Var& CSP::GetVar(VarId vid) const
	{
		if (vid < 0 || vid >= vars.size())
			return null_var;

		return vars[vid];
	}

	bool CSP::ForwardCheckingStep(Assignment& a) const
	{
		if (a.IsComplete())
			return true;

		// add a new saved domain step
		a.saved_domains.push_back(SavedDomainStep());

		VarId vid = a.NextUnassignedVar();
		const Domain& dom = a.GetCurrentDomain(vid);
		const Var& var = GetVar(vid);

		for (int d_idx = 0; d_idx < dom.values.size(); d_idx++)
		{
			int val = dom.values[d_idx];
			if (a.ValidateConstraints(constraints, var, val))
			{
				a.AssignVar(var, val);

				bool success = true;
				for (int c_idx = 0; success && c_idx < var.linked_constraints.size(); c_idx++)
				{
					const Constraint& con = constraints[var.linked_constraints[c_idx]];
					success &= a.AplyArcConsistency(con, var);
				}
				if (success)
				{
					success = ForwardCheckingStep(a);
				}
				if (success)
				{
					return true;
				}
				else
				{
					a.UnAssignVar(var);
					// Restore saved domains
					a.RestoreSavedDomainStep();
				}
			}
		}

		a.saved_domains.pop_back();
		return false;
	}

}; /*namespace ksvd*/

#endif // FCHECK_IMPLEMENTATION