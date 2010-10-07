/*
 * jsconstraintgraph.cpp
 *
 *  Created on: May 6, 2010
 *      Author: rodrigosol
 */
#include <map>
#include <list>
#include <string>
#include <vector>
#include <set>
#include <iostream>
#include <fstream>
#include <sstream>
#include "jsinttypes.h"
#include "jsconstraintgraph.h"

std::string IntToStr(int n) {
	std::ostringstream result;
	result << n;
	return result.str();
}

//void JSVarMap::incSize(JSConstraintVarNode** &array, int &actualCapacity,
//		unsigned blockSize, int var) {
//	unsigned blocks = (((var / blockSize) + 1) * blockSize);
//
//	array = (JSConstraintVarNode **) realloc(array, blocks
//			* sizeof(JSConstraintVarNode*));
//	memset(&array[actualCapacity], NULL, (blocks - actualCapacity)
//			* sizeof(JSConstraintVarNode*));
//
//	actualCapacity = blocks;
//}
//
//void JSVarMap::set(int var, JSConstraintVarNode* node) {
//	if (var >= 0) {
//		if (var >= localsCapacity) {
//			incSize(locals, localsCapacity, JSVarMap::LOCALS_BLOCK_SIZE, var);
//		}
//		locals[var] = node;
//	} else {
//		int newVar = abs(var) - 1;
//		if (newVar >= constantsCapacity) {
//			incSize(constants, constantsCapacity,
//					JSVarMap::CONSTANTS_BLOCK_SIZE, newVar);
//		}
//		constants[newVar] = node;
//	}
//}
//
//JSConstraintVarNode* JSVarMap::get(int var) {
//
//	if (var >= 0) {
//		if (var >= localsCapacity) {
//			return NULL;
//		} else {
//			return locals[var];
//		}
//	} else {
//		int newVar = abs(var) - 1;
//		if (newVar >= constantsCapacity) {
//			return NULL;
//		} else {
//			return constants[newVar];
//		}
//	}
//}

void JSVarMap::set(int var, JSConstraintVarNode* node) {
	map[var] = node;
}

JSConstraintVarNode* JSVarMap::get(int var) {
	JSConstraintVarNode* vn = NULL;
	std::map<int, JSConstraintVarNode*>::iterator it = map.find(var);
	if (it != map.end()) {
		vn = it->second;
	}
	return vn;
}

/*-------------------------JSConstraintNode-----------------------------*/
void JSConstraintNode::updateRemovables(std::vector<nanojit::LIns*> *removables) {
	if (!needsOvfTest()) {
		if (ovs.firstUse()) {
			fprintf(stderr,"\nOV_REMOVIDO\n");
			nanojit::LIns* guard = ovs.getGuard();
			removables->push_back(guard);
		}
	}else{
	 fprintf(stderr,"\nOV_NAO_REMOVIDO\n");
	}

}
std::string JSConstraintVarNode::opcode() {
	return IntToStr(var) + "_" + IntToStr(alias);
}

//std::string JSConstraintVarNode::opcode() {
//	return IntToStr(var) + "_" + IntToStr(alias);
//}


/*-------------------------JSConstraintVarNode--------------------------*/
std::string JSConstraintVarNode::toString() {
	std::string lName = low == MIN_VALUE ? "-INF" : IntToStr(low);
	std::string uName = up == MAX_VALUE ? "+INF" : IntToStr(up);
	return  opcode() + "[" + lName + ", " + uName
			+ "]";
}

std::string JSConstraintVarNode::toDot() {
	std::string ans = "";
	std::string nodeThis = "Node" + IntToStr(id());
	// Print the label of this node:
	ans += "  " + nodeThis + " [ shape = \"box\", label = \"" + toString()
			+ "\" ] ;\n";

	return ans;
}
/*-------------------------JSConstraintCondNode--------------------------*/

std::string JSConstraintCondNode::toString() {
	return "(" + in1->toString() + " " + opcode() + " " + in2->toString()
			+ ")? => (" + out1->toString() + ", " + out2->toString() + ")";
}

std::string JSConstraintCondNode::toDot() {

	std::string ans = "";
	std::string nodeIn1 = "Node" + IntToStr(in1->id());
	std::string nodeIn2 = "Node" + IntToStr(in2->id());
	std::string nodeOut1 = "Node" + IntToStr(out1->id());
	std::string nodeOut2 = "Node" + IntToStr(out2->id());
	std::string nodeThis = "Node" + IntToStr(id());

	// Print the lable of this node:
	ans += "  " + nodeThis + " [ label = \"" + opcode() + "\" ] ;\n";
	// Print the edges:
	ans += "  " + nodeIn1 + " -> " + nodeThis + " ; \n";
	ans += "  " + nodeIn2 + " -> " + nodeThis + " ; \n";
	ans += "  " + nodeThis + " -> " + nodeOut1 + " ; \n";
	ans += "  " + nodeThis + " -> " + nodeOut2 + " ; \n";

	return ans;
}

/*-------------------------JSConstraintBinOpNode--------------------------*/
std::string JSConstraintBinOpNode::toString() {
	return out->toString() + " = " + in1->toString() + " " + opcode() + " "
			+ in2->toString();
}

std::string JSConstraintBinOpNode::toDot() {
	std::string ans = "";
	std::string nodeIn1 = "Node" + IntToStr(in1->id());
	std::string nodeIn2 = "Node" + IntToStr(in2->id());
	std::string nodeOut = "Node" + IntToStr(out->id());
	std::string nodeThis = "Node" + IntToStr(id());

	// Print the lable of this node:
	std::string ovfColor =
			needsOvf ? "style=\"filled,rounded\", fillcolor = \"yellow\", "
					: "";
	ans += "  " + nodeThis + " [ " + ovfColor + " shape=\"box\", label = \""
			+ opcode() + "\" ] ;\n";

	// Print the edges:
	ans += "  " + nodeIn1 + " -> " + nodeThis + " ; \n";
	ans += "  " + nodeIn2 + " -> " + nodeThis + " ; \n";
	ans += "  " + nodeThis + " -> " + nodeOut + " ; \n";

	return ans;
}

/*-------------------------JSConstraintUnOpNode--------------------------*/
std::string JSConstraintUnOpNode::toString() {
	return def->toString() + " = " + use->toString() + opcode();
}

std::string JSConstraintUnOpNode::toDot() {
	std::string ans = "";
	std::string nodeIn = "Node" + IntToStr(use->id());
	std::string nodeOut = "Node" + IntToStr(def->id());
	std::string nodeThis = "Node" + IntToStr(id());

	// Print the lable of this node:
	std::string ovfColor =
			needsOvf ? "style=\"filled,rounded\", fillcolor = \"yellow\", "
					: "";
	ans += "  " + nodeThis + " [ " + ovfColor + " label = \"" + opcode()
			+ "\" ] ;\n";

	// Print the edges:
	ans += "  " + nodeIn + " -> " + nodeThis + " ; \n";
	ans += "  " + nodeThis + " -> " + nodeOut + " ; \n";

	return ans;
}

/*-------------------------JSConstraintDecNode--------------------------*/

void JSConstraintDecNode::update(std::vector<nanojit::LIns*> *removables) {
	int low = use->getLowerLimit();
	int up = use->getUpperLimit();

	if (low == MIN_VALUE) {
	        low = MIN_VALUE + 1;
		markOvf();
	}
	if (up == MIN_VALUE) {
	        up = MIN_VALUE + 1;
		markOvf();
	}
	def->setLimits(low - 1, up - 1);
	updateRemovables(removables);
}

/*-------------------------JSConstraintEqNode--------------------------*/
void JSConstraintEqNode::update(std::vector<nanojit::LIns*> *removables) {
	int up = std::min(in1->getUpperLimit(), in2->getUpperLimit());
	int low = std::max(in1->getLowerLimit(), in2->getLowerLimit());
	out1->setLimits(up, low);
	out2->setLimits(up, low);
}

/*-------------------------JSConstraintGeNode--------------------------*/
void JSConstraintGeNode::update(std::vector<nanojit::LIns*> *removables) {
	int u2 = std::min(in2->getUpperLimit(), in1->getLowerLimit());
	int l1 = std::max(in1->getLowerLimit(), in2->getUpperLimit());
	out1->setLimits(l1, in1->getUpperLimit());
	out2->setLimits(in2->getUpperLimit(), u2);
}

/*-------------------------JSConstraintGtNode--------------------------*/
void JSConstraintGtNode::update(std::vector<nanojit::LIns*> *removables) {
	int u2 = std::min(in2->getUpperLimit(), in1->getLowerLimit() - 1);
	int l1 = std::max(in1->getLowerLimit(), in2->getUpperLimit() + 1);
	out1->setLimits(l1, in1->getUpperLimit());
	out2->setLimits(in2->getUpperLimit(), u2);
}

/*-------------------------JSConstraintIncNode--------------------------*/
void JSConstraintIncNode::update(std::vector<nanojit::LIns*> *removables) {
	int low = use->getLowerLimit();
	int up = use->getUpperLimit();

	if (low == MAX_VALUE) {
	        low = MAX_VALUE - 1;
		markOvf();
	}
	if (up == MAX_VALUE) {
	        up = MAX_VALUE - 1;
		markOvf();
	}
	def->setLimits(low + 1, up + 1);
	updateRemovables(removables);
}

/*-------------------------JSConstraintLeNode--------------------------*/
void JSConstraintLeNode::update(std::vector<nanojit::LIns*> *removables) {
	int u1 = std::min(in1->getUpperLimit(), in2->getLowerLimit());
	int l2 = std::max(in2->getLowerLimit(), in1->getUpperLimit());
	out1->setLimits(in1->getLowerLimit(), u1);
	out2->setLimits(l2, in2->getUpperLimit());
}

/*-------------------------JSConstraintLtNode--------------------------*/
void JSConstraintLtNode::update(std::vector<nanojit::LIns*> *removables) {
	int u1 = std::min(in1->getUpperLimit(), in2->getLowerLimit() - 1);
	int l2 = std::max(in2->getLowerLimit(), in1->getUpperLimit() + 1);
	out1->setLimits(in1->getLowerLimit(), u1);
	out2->setLimits(l2, in2->getUpperLimit());
}

/*-------------------------JSConstraintMoveNode--------------------------*/
void JSConstraintMoveNode::update(std::vector<nanojit::LIns*> *removables) {
	// Find the lower boundary:
	def->setLimits(use->getLowerLimit(), use->getUpperLimit());
}

std::string JSConstraintMoveNode::toDot() {
	std::string ans = "";
	std::string nodeDef = "Node" + IntToStr(def->id());
	std::string nodeUse = "Node" + IntToStr(use->id());
	std::string nodeThis = "Node" + IntToStr(id());

	// Print the lable of this node:
	ans += "  " + nodeThis + " [ label = \" = \" ] ;\n";

	// Print the edges:
	ans += "  " + nodeUse + " -> " + nodeThis + " ; \n";
	ans += "  " + nodeThis + " -> " + nodeDef + " ; \n";

	return ans;
}

/*-------------------------JSConstraintMulNode--------------------------*/
void JSConstraintMulNode::update(std::vector<nanojit::LIns*> *removables) {
	// Find the lower boundary:
	int low1 = in1->getLowerLimit();
	int low2 = in2->getLowerLimit();
	int up1 = in1->getUpperLimit();
	int up2 = in2->getUpperLimit();
	JSInt64 t1 = (JSInt64)low1 * low2;
	JSInt64 t2 = (JSInt64)low1 * up2;
	JSInt64 t3 = (JSInt64)up1 * low2;
	JSInt64 t4 = (JSInt64)up1 * up2;
	JSInt64 low = std::min(t1, std::min(t2, std::min(t3, t4)));

	if (low < MIN_VALUE) {
	        low = MIN_VALUE;
		markOvf();
        } else if (low > MAX_VALUE) {
		low = MAX_VALUE;
		markOvf();
	}

	// Find the upper boundary:
	JSInt64 up =  std::max(t1, std::max(t2, std::max(t3, t4)));
	if (up < MIN_VALUE) {
	        up = MIN_VALUE;
		markOvf();
        } else if (up > MAX_VALUE) {
		up = MAX_VALUE;
		markOvf();
	}

	out->setLimits((int)low, (int)up);
	updateRemovables(removables);
}

/*-------------------------JSConstraintSubNode--------------------------*/
void JSConstraintSubNode::update(std::vector<nanojit::LIns*> *removables) {
	// Find the lower boundary:
	int low1 = in1->getLowerLimit();
	int low2 = in2->getLowerLimit();
	int up1 = in1->getUpperLimit();
	int up2 = in2->getUpperLimit();
	int low;
	if ((unsigned)low1 - up2 > (unsigned)MAX_VALUE && low1 > 0 && up2 < 0) {
		low = MAX_VALUE;
		markOvf();
	} else if ((unsigned)low1 - up2 <= (unsigned)MAX_VALUE && low1 < 0 && up2 > 0) {
		low = MIN_VALUE;
		markOvf();
	} else {
	        low = low1 - up2;
        }

	// Find the upper boundary:
	int up;
	if ((unsigned)up1 - low2 > (unsigned)MAX_VALUE && up1 > 0 && low2 < 0) {
		up = MAX_VALUE;
		markOvf();
	} else if ((unsigned)up1 - low2 <= (unsigned)MAX_VALUE && up1 < 0 && low2 > 0) {
		up = MIN_VALUE;
		markOvf();
	} else {
	        up = up1 - low2;
        }
	out->setLimits(low, up);
	updateRemovables(removables);
}

/*-------------------------JSConstraintAddNode--------------------------*/
void JSConstraintAddNode::update(std::vector<nanojit::LIns*> *removables) {
	// Find the lower boundary:
	int low1 = in1->getLowerLimit();
	int low2 = in2->getLowerLimit();
	int low;
	if ((unsigned)low1 + low2 > (unsigned)MAX_VALUE && low1 > 0 && low2 > 0) {
		low = MAX_VALUE;
		markOvf();
	} else if ((unsigned)low1 + low2 <= (unsigned)MAX_VALUE && low1 < 0 && low2 < 0) {
		low = MIN_VALUE;
		markOvf();
	} else {
	        low = low1 + low2;
	}

	// Find the upper boundary:
	int up1 = in1->getUpperLimit();
	int up2 = in2->getUpperLimit();
	int up;
	if ((unsigned)up1 + up2 > (unsigned)MAX_VALUE && up1 > 0 && up2 > 0) {
		up = MAX_VALUE;
		markOvf();
	} else if ((unsigned)up1 + up2 <= (unsigned)MAX_VALUE && up1 < 0 && up2 < 0) {
		up = MIN_VALUE;
		markOvf();
	} else {
	        up = up1 + up2;
        }

	out->setLimits(low, up);
	updateRemovables(removables);
}

/*-------------------------JSConstraintGraph--------------------------*/

void JSConstraintGraph::propagateConstraints(
		std::vector<nanojit::LIns*> *removables) {
	std::vector<JSConstraintNode*>::iterator it;
	for (it = nodeStack.begin(); it != nodeStack.end(); it++) {
		(*it)->update(removables);
	}
}

JSConstraintVarNode* JSConstraintGraph::addVarNode(int var) {
	return addVarNode(var, JSConstraintNode::MIN_VALUE,
			JSConstraintNode::MAX_VALUE);
}

/*
 * 1) Check if node with key == var already exists in the graph
 * 2) If yes, then update var
 */
JSConstraintVarNode* JSConstraintGraph::addVarNode(int var, int low, int up) {
	JSConstraintVarNode* n = NULL;
	JSConstraintVarNode* it = map.get(var);
	if (it != NULL) {
		n = new JSConstraintVarNode(getID(), var, low, up);
		n->alias = it->alias + 1;
		n->firstAlias = it->firstAlias;

	} else {
		n = new JSConstraintVarNode(getID(), var, low, up);
		n->alias = 0;
		n->firstAlias = n;
	}
	map.set(var, n);
	//nodeStack.push_back(n);
	return n;
}

void JSConstraintGraph::addUnaryArithNode(int var, JSOp op) {
	JSConstraintVarNode* use = map.get(var);

	JSConstraintVarNode* def = addVarNode(var);
	JSConstraintNode* n;
	markAsUpdated(var);
	if (op == JSOP_LOCALINC || op == JSOP_INCLOCAL) {
		n = new JSConstraintIncNode(getID(), def, use);
	} else if (op == JSOP_LOCALDEC || op == JSOP_DECLOCAL) {
		n = new JSConstraintDecNode(getID(), def, use);
	} else {
		fprintf(stderr, "Undefined unary operation: %s", js_CodeName[op]);
		n = NULL;
		exit(1);
	}
	lastAdded = n;
	accessID = n->id();
	nodeStack.push_back(n);
}

void JSConstraintGraph::addBinaryArithNode(int a, int b, int res, JSOp op) {

	JSConstraintVarNode *in1 = map.get(a);
	JSConstraintVarNode *in2 = map.get(b);

	JSConstraintVarNode *out = addVarNode(res);
	JSConstraintNode * n;
	markAsUpdated(res);

	if (op == JSOP_ADD) {
		n = new JSConstraintAddNode(getID(), in1, in2, out);
	} else if (op == JSOP_SUB) {
		n = new JSConstraintSubNode(getID(), in1, in2, out);
	} else if (op == JSOP_MUL) {
		n = new JSConstraintMulNode(getID(), in1, in2, out);
	} else {
		fprintf(stderr, "Undefined unary operation: %s ", js_CodeName[op]);
		n = NULL;
		exit(1);
	}
	accessID = n->id();
	lastAdded = n;
	nodeStack.push_back(n);
}

void JSConstraintGraph::addMoveNode(int defName, int useName) {
	JSConstraintVarNode *use = map.get(useName);
	JSConstraintVarNode *def = addVarNode(defName);

	markAsUpdated(defName);
	JSConstraintNode * n = new JSConstraintMoveNode(getID(), def, use);
	nodeStack.push_back(n);
}

void JSConstraintGraph::addCondNode(int v1, int v2, JSOp op) {
	JSConstraintVarNode *in1 = map.get(v1);
	JSConstraintVarNode *in2 = map.get(v2);

	JSConstraintVarNode *out1 = addVarNode(v1);
	JSConstraintVarNode *out2 = addVarNode(v2);
	JSConstraintNode * condNode;
	if (op == JSOP_EQ) {
		condNode = new JSConstraintEqNode(getID(), in1, in2, out1, out2);
	} else if (op == JSOP_LE) {
		condNode = new JSConstraintLeNode(getID(), in1, in2, out1, out2);
	} else if (op == JSOP_LT) {
		condNode = new JSConstraintLtNode(getID(), in1, in2, out1, out2);
	} else if (op == JSOP_GE) {
		condNode = new JSConstraintGeNode(getID(), in1, in2, out1, out2);
	} else if (op == JSOP_GT) {
		condNode = new JSConstraintGtNode(getID(), in1, in2, out1, out2);
	} else {
		fprintf(stderr, "Undefined unary operation: %s", js_CodeName[op]);
		condNode = NULL;
		exit(1);
	}
	nodeStack.push_back(condNode);
}

std::string JSConstraintGraph::toString() {
	std::string ans = "";
	std::vector<JSConstraintNode*>::iterator it;
	for (it = nodeStack.begin(); it != nodeStack.end(); it++) {
		ans += (*it)->toString() + "\n";
	}
	return ans;
}

void JSConstraintGraph::dot(std::string file) {
	fprintf(stderr, "\nGerando dot:%s", file.c_str());
	FILE *fp;
	fp = fopen(file.c_str(), "w");
	fprintf(fp, "digraph Xulambs {");
	std::vector<JSConstraintNode*>::iterator it;
	for (it = nodeStack.begin(); it != nodeStack.end(); it++) {
		fprintf(fp, "\n%s", (*it)->toDot().c_str());
	}
	fprintf(fp, "}");
	fclose(fp);
}
void JSConstraintGraph::addLIRtoLastNode(nanojit::LIns* guard) {
	if ((lastAdded != 0x0) && accessID == lastAdded->id()) {
		lastAdded->addOvsLIns(guard);
		accessID = 0;
	}
}

void JSConstraintGraph::markAsUpdated(int var) {
	map.get(var)->firstAlias->setLimits(JSConstraintNode::MIN_VALUE,
			JSConstraintNode::MAX_VALUE);
}
/*---------------------------------------------------------------------*/
