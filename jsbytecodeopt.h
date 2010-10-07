#include "jsapi.h"
#include "jsopcode.h"
#include "jstypes.h"
#include "jsinterp.h"
#include "jsvalue.h"
#include "jsconstraintgraph.h"

#ifndef JSBYTECODEOTIMIZATIONS_H_
#define JSBYTECODEOTIMIZATIONS_H_

/**
 * This class works as a bridge between the interpreter and the implementation
 * of our algorithm. This class has two basic functions:
 * - First, it keeps track of the values of each local variable.
 * - Second, it adds to the constraint graph nodes that are created to
 * represent some instructions in the trace. These nodes might denote variables
 * and arithmetic operations.
 */
class JSOverflowOptimization {
private:
	JSContext* cx;
	JSScript *script;
	JSConstraintGraph constraintGraph;

	bool accessFlag;
	bool ignoreNext;
	int constID;
	std::set<int> defineds;
	std::map<JSOp,unsigned> counter;
	int last_var_updated;

	jsbytecode *pc_history[4];

	jsval getLocalValue(jsbytecode* pc);
	jsval getConstantValue(jsbytecode* pc);

	void setPCHistory(jsbytecode* pc);
	void printBytecode(jsbytecode* pc);
	void processHeader();
	void processBytecode();
	bool isConstant(JSOp op); // O(1)
	bool checkOperand(jsbytecode* operand, int &value, int &varName); // O(1)
	void addVar(int name, int value);
	void addVarForce(int name, int value);
	void addFreeVar(int name);
	bool processConstraint(jsbytecode* a, jsbytecode* b, JSOp op);
	bool isConstraintOperator(JSOp op);
	bool isBinOperator(JSOp op);
	int getConstID() {
		return constID--;
	}

public:

	enum STATES {
		READY, HEADER, RECORDING, STOPPED, ABORTED
	};
	STATES internalState;

	JSOverflowOptimization(JSContext* context, JSScript* scr) {
		cx = context;
		script = scr;
		pc_history[3] = pc_history[2] = pc_history[1] = pc_history[0] = 0x0;
		internalState = READY;
		accessFlag = true;
		constID = -1;
	};

	void js_RecorderBytecode(jsbytecode* pc);

	void js_AnaliseBytecode();
	void dot(std::string fileName);
	void getRemovables(std::vector<nanojit::LIns*> *removables);
	void js_AddInstructions(nanojit::LIns* guard) {
		constraintGraph.addLIRtoLastNode(guard);
	}
	void printStats();
};

#endif /* JSBYTECODEOTIMIZATIONS_H_ */
