#include "jsapi.h"
#include "jsopcode.h"
#include "jstypes.h"
#include "jsinterp.h"
#include "jsbit.h"
#include "jsval.h"
#include "jscntxt.h"
#include "jsvector.h"
#include "jsscope.h"

#include "jsbytecodeopt.h"
#include <map>
#include <list>


void JSOverflowOptimization::printStats(){
	std::map<JSOp,unsigned>::iterator it;
	for (it = counter.begin() ; it != counter.end(); it++ ){
		fprintf(stderr,"\nOV_STATS[%s:%d]",js_CodeName[(*it).first],(*it).second);
	}
	counter.clear();
}

void JSOverflowOptimization::setPCHistory(jsbytecode* pc) {

	//     memmove(hist_dest,hist_source,hist_size);
	pc_history[3] = pc_history[2];
	pc_history[2] = pc_history[1];
	pc_history[1] = pc_history[0];

	pc_history[0] = pc;

	//Collect stats from bytecodes
	JSOp op = (JSOp) *pc;
	std::map<JSOp,unsigned>::iterator it = counter.find(op);
	if(it == counter.end()){
		counter[op] = 1;
	}else{
		counter[op]++;
	}

}

jsval JSOverflowOptimization::getLocalValue(jsbytecode* pc) {
	return js::Jsvalify(cx->fp()->slots()[GET_SLOTNO(pc)]);
}

jsval JSOverflowOptimization::getConstantValue(jsbytecode* pc) {
	JSOp op = (JSOp) *pc;
	const JSCodeSpec *cs = &js_CodeSpec[op];
	uint32 type = JOF_TYPE(cs->format);
	jsint i;
	switch (type) {
	default:
		if (op == JSOP_ZERO) {
			i = 0;
		} else if (op == JSOP_ONE) {
			i = 1;
		}
		break;

	case JOF_UINT24:
		i = (jsint) GET_UINT24(pc);
		break;
	case JOF_UINT16:
		i = (jsint) GET_UINT16(pc);
		break;

	case JOF_UINT8:
		i = pc[1];
		break;

	case JOF_INT8:
		i = GET_INT8(pc);
		break;

	case JOF_INT32:
		JS_ASSERT(op == JSOP_INT32);
		i = GET_INT32(pc);
		break;
	}
        return INT_TO_JSVAL(i);
}

void JSOverflowOptimization::js_RecorderBytecode(jsbytecode* pc) {
	setPCHistory(pc);
//	JSOp op = (JSOp) *pc;
//
//
//	if (op == JSOP_SETLOCAL) {
//		JSOp previus = (JSOp) *pc_history[1];
//
//		if (accessFlag) {
//			int name = GET_SLOTNO(pc_history[0]);
//			jsval val = getLocalValue(pc_history[0]);
//			if (JSVAL_IS_INT(val)){
//				int value = JSVAL_TO_INT(getLocalValue(pc_history[0]));
//
//				if (previus == JSOP_GETLOCAL) {
//					int pName, pValue;
//					pName = GET_SLOTNO(pc_history[1]);
//					jsval pVal = getLocalValue(pc_history[1]);
//                                        if(JSVAL_IS_INT(pVal)){
//                                                pValue = JSVAL_TO_INT(getLocalValue(pc_history[1]));
//					        addVar(name, value);
//					        addVar(pName, pValue);
//					        constraintGraph.addMoveNode(name, pName);
//                                        }
//				} else if (isConstant(previus)) {
//
//					jsval pVal = getLocalValue(pc_history[1]);
//                                        if(JSVAL_IS_INT(pVal)){
//                                                value = JSVAL_TO_INT(getConstantValue(pc_history[1]));
//				        	addVarForce(name, value);
//                                        }
//				} else if (isBinOperator(previus)) {
//					addVarForce(name, value);
//				} else {
//					addFreeVar(name);
//				}
//			}
//		}
//		accessFlag = true;
//	}
}

void JSOverflowOptimization::js_AnaliseBytecode() {
	//printBytecode(getPCHistory(0));

	if (internalState == READY) {
		//        printBytecode(getPCHistory(0));
		processHeader();
	} else if (internalState == RECORDING) {
		//        printBytecode(getPCHistory(0));
		processBytecode();
	}

}
bool JSOverflowOptimization::processConstraint(jsbytecode* a, jsbytecode* b,
		JSOp op) {
	int nameA, nameB, valueA, valueB;
	if (isConstraintOperator(op) && checkOperand(a, valueA, nameA)
			&& checkOperand(b, valueB, nameB)) {
		addVar(nameB, valueB);
		addVar(nameA, valueA);
		constraintGraph.addCondNode(nameB, nameA, op);
		accessFlag = false;
		return true;
	}else{
		return false;
	}

}

bool JSOverflowOptimization::isConstraintOperator(JSOp op) {
	return (op == JSOP_LT || op == JSOP_LE || op == JSOP_GT || op == JSOP_GE
			|| op == JSOP_EQ);
}

bool JSOverflowOptimization::isBinOperator(JSOp op) {
	return (op == JSOP_ADD || op == JSOP_SUB || op == JSOP_MUL);
}
void JSOverflowOptimization::processHeader() {

	JSOp op = (JSOp) *pc_history[1];
	if(isConstraintOperator(op)){
		jsbytecode* a = pc_history[2];
		jsbytecode* b = pc_history[3];
		if(processConstraint(a, b, op)){
			internalState = RECORDING;
			accessFlag = true;
		}
	}
}

bool JSOverflowOptimization::checkOperand(jsbytecode* operand, int &value,
		int &varName) {
	JSOp op = (JSOp) (*operand);
	if (op == JSOP_GETLOCAL) {
		varName = GET_SLOTNO(operand);
		jsval val = getLocalValue(operand);
		if (JSVAL_IS_INT(val)) {
			value = JSVAL_TO_INT(val);
			return true;
		} else {
			return false;
		}
	} else if (isConstant(op)) {
		varName = getConstID();
		value = JSVAL_TO_INT(getConstantValue(operand));
		return true;
	}
	return false;
}

// TODO: see if it is possible to eliminate the double search for the variable. This might be
// done by modifying the interface of the methods that add arith/rel/move nodes to the CG.
void JSOverflowOptimization::processBytecode() {
	JSOp op = (JSOp) *pc_history[0];

	if (op == JSOP_LOCALINC || op == JSOP_INCLOCAL || op == JSOP_LOCALDEC || op
			== JSOP_DECLOCAL) {
		accessFlag = true;
		int name, value;
		name = GET_SLOTNO(pc_history[0]);
		jsval v =  getLocalValue(pc_history[0]);
		if(JSVAL_IS_INT(v)){

		       // jsval pVal = getLocalValue(pc_history[1]);
                //        if(JSVAL_IS_INT(pVal)){
                                value = JSVAL_TO_INT(getLocalValue(pc_history[0]));
			        addVar(name, value);
			        constraintGraph.addUnaryArithNode(name, op);
			        accessFlag = true;
                  //     }
		}
	} else if (isBinOperator(op)) {

		const JSCodeSpec *cs = &js_CodeSpec[op];
		jsbytecode* a = (pc_history[0] + cs->length);
		jsbytecode* b = pc_history[1];
		jsbytecode* c = pc_history[2];
		int nameA, nameB, nameC, valueA, valueB, valueC;
		if (checkOperand(b, valueB, nameB) && checkOperand(c, valueC, nameC)) {
			JSOp opA = (JSOp) *a;
			if (opA == JSOP_SETLOCAL) {
				nameA = GET_SLOTNO(a);
				jsval v = getLocalValue(a);
				if(JSVAL_IS_INT(v)){
					valueA = JSVAL_TO_INT(getLocalValue(a));
					addVar(nameA, valueA);
					addVar(nameB, valueB);
					addVar(nameC, valueC);

					constraintGraph.addBinaryArithNode(nameB, nameC, nameA, op);
				}
			}
		}else {
			fprintf(stderr,"\nOV_GLOBAL");
		}
		accessFlag = false;
	} else if (isConstraintOperator(op)) {
		jsbytecode* a = pc_history[1];
		jsbytecode* b = pc_history[2];
		processConstraint(a, b, op);
		accessFlag = true;
		//    }else if(op == JSOP_NAMEINC || op == JSOP_NAMEDEC || op == JSOP_DECNAME || op == JSOP_INCNAME){
		//        accessFlag = true;
		//        fprintf(stderr,"\nOV_GLOBAL");
	} else {
		accessFlag = true;
	}
}

bool JSOverflowOptimization::isConstant(JSOp op) {

	const JSCodeSpec *cs = &js_CodeSpec[op];
	uint32 type = JOF_TYPE(cs->format);

	if (type == JOF_UINT24 || type == JOF_UINT16 || type == JOF_UINT8 || type
			== JOF_INT8 || type == JOF_INT32 || op == JSOP_ZERO || op
			== JSOP_ONE) {
		return true;
	}
	return false;

}

void JSOverflowOptimization::addVar(int name, int value) {
	std::set<int>::iterator it;
	it = defineds.find(name);
	if (it == defineds.end()) {
		constraintGraph.addVarNode(name, value, value);
		defineds.insert(name);
	}

}
void JSOverflowOptimization::addVarForce(int name, int value) {
	constraintGraph.addVarNode(name, value, value);
	defineds.insert(name);
}

void JSOverflowOptimization::addFreeVar(int name) {
	constraintGraph.addVarNode(name);
	defineds.insert(name);
}

void JSOverflowOptimization::dot(std::string fileName) {
	constraintGraph.dot(fileName);
}

void JSOverflowOptimization::getRemovables(
		std::vector<nanojit::LIns*> *removables) {
	constraintGraph.propagateConstraints(removables);
	internalState = READY;
	//    std::set<int>::iterator it;
	//    for(it = defineds.begin(); it != defineds.end(); it++){
	//        addFreeVar((*it));
	//    }
}
