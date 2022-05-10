## 本体はtoolbox/makefile

.PHONY : clean test RELEASE DEBUG COVERAGE


clean test RELEASE DEBUG COVERAGE:
	@make -r -j -f toolbox/makefile $(MAKECMDGOALS)
