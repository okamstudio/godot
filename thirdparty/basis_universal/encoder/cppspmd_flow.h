// Do not include this header directly.
// Control flow functionality in common between all the headers.
//
// Copyright 2020-2024 Binomial LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifdef _DEBUG
CPPSPMD_FORCE_INLINE void spmd_kernel::check_masks()
{
	assert(!any(andnot(m_kernel_exec, m_exec)));
}
#endif

CPPSPMD_FORCE_INLINE void spmd_kernel::spmd_break()
{
#ifdef _DEBUG
	assert(m_in_loop);
#endif

	m_exec = exec_mask::all_off();
}

CPPSPMD_FORCE_INLINE void spmd_kernel::spmd_continue()
{
#ifdef _DEBUG
	assert(m_in_loop);
#endif

	// Kill any active lanes, and remember which lanes were active so we can re-enable them at the end of the loop body.
	m_continue_mask = m_continue_mask | m_exec;
	m_exec = exec_mask::all_off();
}

CPPSPMD_FORCE_INLINE void spmd_kernel::spmd_return()
{
	// Permenantly kill all active lanes
	m_kernel_exec = andnot(m_exec, m_kernel_exec);
	m_exec = exec_mask::all_off();
}
			
template<typename UnmaskedBody>
CPPSPMD_FORCE_INLINE void spmd_kernel::spmd_unmasked(const UnmaskedBody& unmaskedBody)
{
	exec_mask orig_exec = m_exec, orig_kernel_exec = m_kernel_exec;

	m_kernel_exec = exec_mask::all_on();
	m_exec = exec_mask::all_on();

	unmaskedBody();

	m_kernel_exec = m_kernel_exec & orig_kernel_exec;
	m_exec = m_exec & orig_exec;
	
	check_masks();
}

struct scoped_unmasked_restorer
{
	spmd_kernel *m_pKernel;
	exec_mask m_orig_exec, m_orig_kernel_exec;
				
	CPPSPMD_FORCE_INLINE scoped_unmasked_restorer(spmd_kernel *pKernel) : 
		m_pKernel(pKernel), 
		m_orig_exec(pKernel->m_exec),
		m_orig_kernel_exec(pKernel->m_kernel_exec)
	{
		pKernel->m_kernel_exec = exec_mask::all_on();
		pKernel->m_exec = exec_mask::all_on();
	}

	CPPSPMD_FORCE_INLINE ~scoped_unmasked_restorer() 
	{ 
		m_pKernel->m_kernel_exec = m_pKernel->m_kernel_exec & m_orig_kernel_exec;
		m_pKernel->m_exec = m_pKernel->m_exec & m_orig_exec;
		m_pKernel->check_masks();
	}
};

#define SPMD_UNMASKED_BEGIN { scoped_unmasked_restorer _unmasked_restorer(this); 
#define SPMD_UNMASKED_END }

#if 0
template<typename SPMDKernel, typename... Args>
CPPSPMD_FORCE_INLINE decltype(auto) spmd_kernel::spmd_call(Args&&... args)
{
	SPMDKernel kernel;
	kernel.init(m_exec);
	return kernel._call(std::forward<Args>(args)...);
}
#else
template<typename SPMDKernel, typename... Args>
CPPSPMD_FORCE_INLINE void spmd_kernel::spmd_call(Args&&... args)
{
	SPMDKernel kernel;
	kernel.init(m_exec);
	kernel._call(std::forward<Args>(args)...);
}
#endif

CPPSPMD_FORCE_INLINE void spmd_kernel::spmd_if_break(const vbool& cond)
{
#ifdef _DEBUG
	assert(m_in_loop);
#endif
	
	exec_mask cond_exec(cond);
					
	m_exec = andnot(m_exec & cond_exec, m_exec);

	check_masks();
}

// No SPMD breaks, continues, etc. allowed
template<typename IfBody>
CPPSPMD_FORCE_INLINE void spmd_kernel::spmd_sif(const vbool& cond, const IfBody& ifBody)
{
	exec_mask im = m_exec & exec_mask(cond);

	if (any(im))
	{
		const exec_mask orig_exec = m_exec;
		m_exec = im;
		ifBody();
		m_exec = orig_exec;
	}
}

// No SPMD breaks, continues, etc. allowed
template<typename IfBody, typename ElseBody>
CPPSPMD_FORCE_INLINE void spmd_kernel::spmd_sifelse(const vbool& cond, const IfBody& ifBody, const ElseBody &elseBody)
{
	const exec_mask orig_exec = m_exec;

	exec_mask im = m_exec & exec_mask(cond);

	if (any(im))
	{
		m_exec = im;
		ifBody();
	}

	exec_mask em = orig_exec & exec_mask(!cond);

	if (any(em))
	{
		m_exec = em;
		elseBody();
	}
		
	m_exec = orig_exec;
}

template<typename IfBody>
CPPSPMD_FORCE_INLINE void spmd_kernel::spmd_if(const vbool& cond, const IfBody& ifBody)
{
	exec_mask cond_exec(cond);
		
	exec_mask pre_if_exec = cond_exec & m_exec;

	if (any(pre_if_exec))
	{
		exec_mask unexecuted_lanes = andnot(cond_exec, m_exec);
		m_exec = pre_if_exec;

		ifBody();

		// Propagate any lanes that got disabled inside the if body into the exec mask outside the if body, but turn on any lanes that didn't execute inside the if body.
		m_exec = m_exec | unexecuted_lanes;

		check_masks();
	}
}

template<typename IfBody, typename ElseBody>
CPPSPMD_FORCE_INLINE void spmd_kernel::spmd_ifelse(const vbool& cond, const IfBody& ifBody, const ElseBody& elseBody)
{
	bool all_flag = false;

	exec_mask cond_exec(cond);
		
	{
		exec_mask pre_if_exec = cond_exec & m_exec;

		int mask = pre_if_exec.get_movemask();
		if (mask != 0)
		{
			all_flag = ((uint32_t)mask == m_exec.get_movemask());

			exec_mask unexecuted_lanes = andnot(cond_exec, m_exec);
			m_exec = pre_if_exec;

			ifBody();

			// Propagate any lanes that got disabled inside the if body into the exec mask outside the if body, but turn on any lanes that didn't execute inside the if body.
			m_exec = m_exec | unexecuted_lanes;

			check_masks();
		}
	}

	if (!all_flag)
	{
		exec_mask pre_if_exec = andnot(cond_exec, m_exec);

		if (any(pre_if_exec))
		{
			exec_mask unexecuted_lanes = cond_exec & m_exec;
			m_exec = pre_if_exec;

			ifBody();

			// Propagate any lanes that got disabled inside the if body into the exec mask outside the if body, but turn on any lanes that didn't execute inside the if body.
			m_exec = m_exec | unexecuted_lanes;

			check_masks();
		}
	}
}

struct scoped_exec_restorer
{
	exec_mask *m_pMask;
	exec_mask m_prev_mask;
	CPPSPMD_FORCE_INLINE scoped_exec_restorer(exec_mask *pExec_mask) : m_pMask(pExec_mask), m_prev_mask(*pExec_mask) { }
	CPPSPMD_FORCE_INLINE ~scoped_exec_restorer() { *m_pMask = m_prev_mask; }
};

// Cannot use SPMD break, continue, or return inside "simple" if/else
#define SPMD_SIF(cond) exec_mask CPPSPMD_GLUER2(_exec_temp, __LINE__)(m_exec & exec_mask(vbool(cond))); if (any(CPPSPMD_GLUER2(_exec_temp, __LINE__))) \
	{ CPPSPMD::scoped_exec_restorer CPPSPMD_GLUER2(_exec_restore_, __LINE__)(&m_exec); m_exec = CPPSPMD_GLUER2(_exec_temp, __LINE__);

#define SPMD_SELSE(cond) } exec_mask CPPSPMD_GLUER2(_exec_temp, __LINE__)(m_exec & exec_mask(!vbool(cond))); if (any(CPPSPMD_GLUER2(_exec_temp, __LINE__))) \
	{ CPPSPMD::scoped_exec_restorer CPPSPMD_GLUER2(_exec_restore_, __LINE__)(&m_exec); m_exec = CPPSPMD_GLUER2(_exec_temp, __LINE__);

#define SPMD_SENDIF }

// Same as SPMD_SIF, except doesn't use a scoped object
#define SPMD_SIF2(cond) exec_mask CPPSPMD_GLUER2(_exec_temp, __LINE__)(m_exec & exec_mask(vbool(cond))); if (any(CPPSPMD_GLUER2(_exec_temp, __LINE__))) \
	{ exec_mask _orig_exec = m_exec; m_exec = CPPSPMD_GLUER2(_exec_temp, __LINE__);

#define SPMD_SELSE2(cond) m_exec = _orig_exec; } exec_mask CPPSPMD_GLUER2(_exec_temp, __LINE__)(m_exec & exec_mask(!vbool(cond))); if (any(CPPSPMD_GLUER2(_exec_temp, __LINE__))) \
	{ exec_mask _orig_exec = m_exec; m_exec = CPPSPMD_GLUER2(_exec_temp, __LINE__);

#define SPMD_SEND_IF2 m_exec = _orig_exec; }

// Same as SPMD_SIF(), except the if/else blocks are always executed
#define SPMD_SAIF(cond) exec_mask CPPSPMD_GLUER2(_exec_temp, __LINE__)(m_exec & exec_mask(vbool(cond))); { CPPSPMD::scoped_exec_restorer CPPSPMD_GLUER2(_exec_restore_, __LINE__)(&m_exec); \
	m_exec = CPPSPMD_GLUER2(_exec_temp, __LINE__);

#define SPMD_SAELSE(cond) } exec_mask CPPSPMD_GLUER2(_exec_temp, __LINE__)(m_exec & exec_mask(!vbool(cond))); { CPPSPMD::scoped_exec_restorer CPPSPMD_GLUER2(_exec_restore_, __LINE__)(&m_exec); \
	m_exec = CPPSPMD_GLUER2(_exec_temp, __LINE__);

#define SPMD_SAENDIF }

// Cannot use SPMD break, continue, or return inside sselect
#define SPMD_SSELECT(var)		do { vint_t _select_var = var; scoped_exec_restorer _orig_exec(&m_exec); exec_mask _select_executed(exec_mask::all_off());
#define SPMD_SCASE(value)		exec_mask CPPSPMD_GLUER2(_exec_temp, __LINE__)(_orig_exec.m_prev_mask & exec_mask(vbool(_select_var == (value)))); if (any(CPPSPMD_GLUER2(_exec_temp, __LINE__))) \
	{ m_exec = CPPSPMD_GLUER2(_exec_temp, __LINE__); _select_executed = _select_executed | m_exec;

//#define SPMD_SCASE_END			if (_select_executed.get_movemask() == _orig_exec.m_prev_mask.get_movemask()) break; }
#define SPMD_SCASE_END			if (!any(_select_executed ^ _orig_exec.m_prev_mask)) break; }
#define SPMD_SDEFAULT			exec_mask _all_other_lanes(andnot(_select_executed, _orig_exec.m_prev_mask)); if (any(_all_other_lanes)) { m_exec = _all_other_lanes;
#define SPMD_SDEFAULT_END		}
#define SPMD_SSELECT_END		} while(0);

// Same as SPMD_SSELECT, except all cases are executed.
// Cannot use SPMD break, continue, or return inside sselect
#define SPMD_SASELECT(var)		do { vint_t _select_var = var; scoped_exec_restorer _orig_exec(&m_exec); exec_mask _select_executed(exec_mask::all_off());

#define SPMD_SACASE(value)		exec_mask CPPSPMD_GLUER2(_exec_temp, __LINE__)(_orig_exec.m_prev_mask & exec_mask(vbool(_select_var == (value)))); { m_exec = CPPSPMD_GLUER2(_exec_temp, __LINE__); \
	_select_executed = _select_executed | m_exec;

#define SPMD_SACASE_END			}
#define SPMD_SADEFAULT			exec_mask _all_other_lanes(andnot(_select_executed, _orig_exec.m_prev_mask)); { m_exec = _all_other_lanes;
#define SPMD_SADEFAULT_END		}
#define SPMD_SASELECT_END		} while(0);

struct scoped_exec_restorer2
{
	spmd_kernel *m_pKernel;
	exec_mask m_unexecuted_lanes;
		
	CPPSPMD_FORCE_INLINE scoped_exec_restorer2(spmd_kernel *pKernel, const vbool &cond) : 
		m_pKernel(pKernel)
	{ 
		exec_mask cond_exec(cond);
		m_unexecuted_lanes = andnot(cond_exec, pKernel->m_exec);
		pKernel->m_exec = cond_exec & pKernel->m_exec;
	}

	CPPSPMD_FORCE_INLINE ~scoped_exec_restorer2() 
	{ 
		m_pKernel->m_exec = m_pKernel->m_exec | m_unexecuted_lanes;
		m_pKernel->check_masks();
	}
};

#define SPMD_IF(cond) { CPPSPMD::scoped_exec_restorer2 CPPSPMD_GLUER2(_exec_restore2_, __LINE__)(this, vbool(cond)); if (any(m_exec)) {
#define SPMD_ELSE(cond) } } { CPPSPMD::scoped_exec_restorer2 CPPSPMD_GLUER2(_exec_restore2_, __LINE__)(this, !vbool(cond)); if (any(m_exec)) {
#define SPMD_END_IF } }

// Same as SPMD_IF, except the conditional block is always executed.
#define SPMD_AIF(cond) { CPPSPMD::scoped_exec_restorer2 CPPSPMD_GLUER2(_exec_restore2_, __LINE__)(this, vbool(cond)); {
#define SPMD_AELSE(cond) } } { CPPSPMD::scoped_exec_restorer2 CPPSPMD_GLUER2(_exec_restore2_, __LINE__)(this, !vbool(cond)); {
#define SPMD_AEND_IF } }

class scoped_exec_saver
{
	exec_mask m_exec, m_kernel_exec, m_continue_mask;
	spmd_kernel *m_pKernel;
#ifdef _DEBUG
	bool m_in_loop;
#endif

public:
	inline scoped_exec_saver(spmd_kernel *pKernel) :
		m_exec(pKernel->m_exec), m_kernel_exec(pKernel->m_kernel_exec), m_continue_mask(pKernel->m_continue_mask),
		m_pKernel(pKernel)
	{ 
#ifdef _DEBUG
		m_in_loop = pKernel->m_in_loop;
#endif
	}
		
	inline ~scoped_exec_saver()
	{ 
		m_pKernel->m_exec = m_exec; 
		m_pKernel->m_continue_mask = m_continue_mask; 
		m_pKernel->m_kernel_exec = m_kernel_exec; 
#ifdef _DEBUG
		m_pKernel->m_in_loop = m_in_loop;
		m_pKernel->check_masks();
#endif
	}
};

#define SPMD_BEGIN_CALL scoped_exec_saver CPPSPMD_GLUER2(_begin_call_scoped_exec_saver, __LINE__)(this); m_continue_mask = exec_mask::all_off();
#define SPMD_BEGIN_CALL_ALL_LANES scoped_exec_saver CPPSPMD_GLUER2(_begin_call_scoped_exec_saver, __LINE__)(this); m_exec = exec_mask::all_on(); m_continue_mask = exec_mask::all_off();

template<typename ForeachBody>
CPPSPMD_FORCE_INLINE void spmd_kernel::spmd_foreach(int begin, int end, const ForeachBody& foreachBody)
{
	if (begin == end)
		return;
	
	if (!any(m_exec))
		return;

	// We don't support iterating backwards.
	if (begin > end)
		std::swap(begin, end);

	exec_mask prev_continue_mask = m_continue_mask, prev_exec = m_exec;
	
	int total_full = (end - begin) / PROGRAM_COUNT;
	int total_partial = (end - begin) % PROGRAM_COUNT;

	lint_t loop_index = begin + program_index;
	
	const int total_loops = total_full + (total_partial ? 1 : 0);

	m_continue_mask = exec_mask::all_off();

	for (int i = 0; i < total_loops; i++)
	{
		int n = PROGRAM_COUNT;
		if ((i == (total_loops - 1)) && (total_partial))
		{
			exec_mask partial_mask = exec_mask(vint_t(total_partial) > vint_t(program_index));
			m_exec = m_exec & partial_mask;
			n = total_partial;
		}

		foreachBody(loop_index, n);

		m_exec = m_exec | m_continue_mask;
		if (!any(m_exec))
			break;

		m_continue_mask = exec_mask::all_off();
		check_masks();
				
		store_all(loop_index, loop_index + PROGRAM_COUNT);
	}

	m_exec = prev_exec & m_kernel_exec;
	m_continue_mask = prev_continue_mask;
	check_masks();
}

template<typename WhileCondBody, typename WhileBody>
CPPSPMD_FORCE_INLINE void spmd_kernel::spmd_while(const WhileCondBody& whileCondBody, const WhileBody& whileBody)
{
	exec_mask orig_exec = m_exec;

	exec_mask orig_continue_mask = m_continue_mask;
	m_continue_mask = exec_mask::all_off();

#ifdef _DEBUG
	const bool prev_in_loop = m_in_loop;
	m_in_loop = true;
#endif

	while(true)
	{
		exec_mask cond_exec = exec_mask(whileCondBody());
		m_exec = m_exec & cond_exec;

		if (!any(m_exec))
			break;

		whileBody();

		m_exec = m_exec | m_continue_mask;
		m_continue_mask = exec_mask::all_off();
		check_masks();
	}

#ifdef _DEBUG
	m_in_loop = prev_in_loop;
#endif

	m_exec = orig_exec & m_kernel_exec;
	m_continue_mask = orig_continue_mask;
	check_masks();
}

struct scoped_while_restorer
{
	spmd_kernel *m_pKernel;
	exec_mask m_orig_exec, m_orig_continue_mask;
#ifdef _DEBUG
	bool m_prev_in_loop;
#endif
				
	CPPSPMD_FORCE_INLINE scoped_while_restorer(spmd_kernel *pKernel) : 
		m_pKernel(pKernel), 
		m_orig_exec(pKernel->m_exec),
		m_orig_continue_mask(pKernel->m_continue_mask)
	{
		pKernel->m_continue_mask.all_off();

#ifdef _DEBUG
		m_prev_in_loop = pKernel->m_in_loop;
		pKernel->m_in_loop = true;
#endif
	}

	CPPSPMD_FORCE_INLINE ~scoped_while_restorer() 
	{ 
		m_pKernel->m_exec = m_orig_exec & m_pKernel->m_kernel_exec;
		m_pKernel->m_continue_mask = m_orig_continue_mask;
#ifdef _DEBUG
		m_pKernel->m_in_loop = m_prev_in_loop;
		m_pKernel->check_masks();
#endif
	}
};

#undef SPMD_WHILE
#undef SPMD_WEND
#define SPMD_WHILE(cond) { scoped_while_restorer CPPSPMD_GLUER2(_while_restore_, __LINE__)(this); while(true) { exec_mask CPPSPMD_GLUER2(cond_exec, __LINE__) = exec_mask(vbool(cond)); \
	m_exec = m_exec & CPPSPMD_GLUER2(cond_exec, __LINE__); if (!any(m_exec)) break;

#define SPMD_WEND m_exec = m_exec | m_continue_mask; m_continue_mask = exec_mask::all_off(); check_masks(); } }

// Nesting is not supported (although it will compile, but the results won't make much sense).
#define SPMD_FOREACH(loop_var, bi, ei) if (((bi) != (ei)) && (any(m_exec))) { \
	scoped_while_restorer CPPSPMD_GLUER2(_while_restore_, __LINE__)(this); \
	uint32_t b = (uint32_t)(bi), e = (uint32_t)(ei); if ((b) > (e)) { std::swap(b, e); } const uint32_t total_full = ((e) - (b)) >> PROGRAM_COUNT_SHIFT, total_partial = ((e) - (b)) & (PROGRAM_COUNT - 1); \
	lint_t loop_var = program_index + (int)b; const uint32_t total_loops = total_full + (total_partial ? 1U : 0U); \
	for (uint32_t CPPSPMD_GLUER2(_foreach_counter, __LINE__) = 0; CPPSPMD_GLUER2(_foreach_counter, __LINE__) < total_loops; ++CPPSPMD_GLUER2(_foreach_counter, __LINE__)) { \
		if ((CPPSPMD_GLUER2(_foreach_counter, __LINE__) == (total_loops - 1)) && (total_partial)) { exec_mask partial_mask = exec_mask(vint_t((int)total_partial) > vint_t(program_index)); m_exec = m_exec & partial_mask; }

#define SPMD_FOREACH_END(loop_var) m_exec = m_exec | m_continue_mask; if (!any(m_exec)) break; m_continue_mask = exec_mask::all_off(); check_masks(); store_all(loop_var, loop_var + PROGRAM_COUNT); } }

// Okay to use spmd_continue or spmd_return, but not spmd_break
#define SPMD_FOREACH_ACTIVE(index_var) int64_t index_var; { uint64_t _movemask = m_exec.get_movemask(); if (_movemask) { scoped_while_restorer CPPSPMD_GLUER2(_while_restore_, __LINE__)(this); \
	for (uint32_t _i = 0; _i < PROGRAM_COUNT; ++_i) { \
		if (_movemask & (1U << _i)) { \
			m_exec.enable_lane(_i); m_exec = m_exec & m_kernel_exec; \
			(index_var) = _i; \

#define SPMD_FOREACH_ACTIVE_END } } } }

// Okay to use spmd_continue, but not spmd_break/spmd_continue
#define SPMD_FOREACH_UNIQUE_INT(index_var, var) { scoped_while_restorer CPPSPMD_GLUER2(_while_restore_, __LINE__)(this); \
	CPPSPMD_DECL(int_t, _vals[PROGRAM_COUNT]); store_linear_all(_vals, var); std::sort(_vals, _vals + PROGRAM_COUNT); \
	const int _n = (int)(std::unique(_vals, _vals + PROGRAM_COUNT) - _vals); \
	for (int _i = 0; _i < _n; ++_i) { int index_var = _vals[_i]; vbool cond = (vint_t(var) == vint_t(index_var)); m_exec = exec_mask(cond);

#define SPMD_FOREACH_UNIQUE_INT_END } }

struct scoped_simple_while_restorer
{
	spmd_kernel* m_pKernel;
	exec_mask m_orig_exec;
#ifdef _DEBUG
	bool m_prev_in_loop;
#endif

	CPPSPMD_FORCE_INLINE scoped_simple_while_restorer(spmd_kernel* pKernel) :
		m_pKernel(pKernel),
		m_orig_exec(pKernel->m_exec)
	{
			
#ifdef _DEBUG
		m_prev_in_loop = pKernel->m_in_loop;
		pKernel->m_in_loop = true;
#endif
	}

	CPPSPMD_FORCE_INLINE ~scoped_simple_while_restorer()
	{
		m_pKernel->m_exec = m_orig_exec;
#ifdef _DEBUG
		m_pKernel->m_in_loop = m_prev_in_loop;
		m_pKernel->check_masks();
#endif
	}
};

// Cannot use SPMD break, continue, or return inside simple while

#define SPMD_SWHILE(cond) { scoped_simple_while_restorer CPPSPMD_GLUER2(_while_restore_, __LINE__)(this); \
	while(true) { \
		exec_mask CPPSPMD_GLUER2(cond_exec, __LINE__) = exec_mask(vbool(cond)); m_exec = m_exec & CPPSPMD_GLUER2(cond_exec, __LINE__); if (!any(m_exec)) break;
#define SPMD_SWEND } }	

// Cannot use SPMD break, continue, or return inside simple do
#define SPMD_SDO { scoped_simple_while_restorer CPPSPMD_GLUER2(_while_restore_, __LINE__)(this); while(true) {
#define SPMD_SEND_DO(cond) exec_mask CPPSPMD_GLUER2(cond_exec, __LINE__) = exec_mask(vbool(cond)); m_exec = m_exec & CPPSPMD_GLUER2(cond_exec, __LINE__); if (!any(m_exec)) break; } }	

#undef SPMD_FOR
#undef SPMD_END_FOR
#define SPMD_FOR(for_init, for_cond) { for_init; scoped_while_restorer CPPSPMD_GLUER2(_while_restore_, __LINE__)(this); while(true) { exec_mask CPPSPMD_GLUER2(cond_exec, __LINE__) = exec_mask(vbool(for_cond)); \
	m_exec = m_exec & CPPSPMD_GLUER2(cond_exec, __LINE__); if (!any(m_exec)) break;
#define SPMD_END_FOR(for_inc) m_exec = m_exec | m_continue_mask; m_continue_mask = exec_mask::all_off(); check_masks(); for_inc; } }
		
template<typename ForInitBody, typename ForCondBody, typename ForIncrBody, typename ForBody>
CPPSPMD_FORCE_INLINE void spmd_kernel::spmd_for(const ForInitBody& forInitBody, const ForCondBody& forCondBody, const ForIncrBody& forIncrBody, const ForBody& forBody)
{
	exec_mask orig_exec = m_exec;

	forInitBody();

	exec_mask orig_continue_mask = m_continue_mask;
	m_continue_mask = exec_mask::all_off();

#ifdef _DEBUG
	const bool prev_in_loop = m_in_loop;
	m_in_loop = true;
#endif

	while(true)
	{
		exec_mask cond_exec = exec_mask(forCondBody());
		m_exec = m_exec & cond_exec;

		if (!any(m_exec))
			break;

		forBody();

		m_exec = m_exec | m_continue_mask;
		m_continue_mask = exec_mask::all_off();
		check_masks();
			
		forIncrBody();
	}

	m_exec = orig_exec & m_kernel_exec;
	m_continue_mask = orig_continue_mask;

#ifdef _DEBUG
	m_in_loop = prev_in_loop;
	check_masks();
#endif
}
