/*
 * jsconstraintgraph.h
 * This file contains the declaration of a constraint graph. This data
 * structure contains nodes for each relational operation that we find
 * in the trace, e.g <, <=, ==, >, >=, and also nodes for five arithmetic
 * operations: add, sub, inc, dec and mul. Once we have built the contraint
 * graph, we traverse it from top to bottom, propagating the ranges of the
 * variables. During the traversal we remove overflows from operations that
 * we can prove that use limits that will not go beyond the range of integers.
 *
 */
#include "nanojit/nanojit.h"

#include <map>
#include <list>
#include <string>
#include <vector>
#include <set>
#include "jsapi.h"
#include "jsopcode.h"

#ifndef JSCONSTRAINTGRAPH_H_

/*
 * This class works as a bridge between the implementation of the overflow
 * elimination analysis and nanojit. Each instance of this class is bound to
 * a arithmetic node. These are the only nodes that might cause overflow tests
 * in the machine code produced by TraceMonkey. For each arithmetic node we
 * keep the instructions in the LIR that will be removed in case we can prove
 * that the overflow test is redundant.
 */

class JSConstraintOverflowable {
private:
	bool removed;
	nanojit::LIns *guard;

public:

	JSConstraintOverflowable() :
		removed(false), guard(0x0) {
	}

	void add(nanojit::LIns* _guard) {
		guard = _guard;
	}
	/*
	 * This method is used to guarantee that only one overflow test will be
	 * eliminated per arithmetic node. We recycle the contraint
	 * graph in the analysis of traces that have been created after the
	 * first time the analysis runs. Thus, there is the possibility that
	 * the same LIR address be seen twice or more times by subsequent runs
	 * of our analysis. Hence, we use this method to test if the overflow
	 * associated to a certain node has been removed or not.
	 */

	bool firstUse() {
		if (!removed && guard != 0x0) {
			removed = true;
			return removed;
		}
		return false;
	}
	nanojit::LIns* getGuard() {
		return guard;
	}

};
/*
 * This is the base class of every node that is part of the contraint graph.
 */
class JSConstraintNode {
private:
	// Each node has a unique id, that we use to renderize its node in dot
	// format.
	unsigned localID;

protected:

	JSConstraintOverflowable ovs;
	void updateRemovables(std::vector<nanojit::LIns*> *removables);

public:

	unsigned alias;

	static const int MIN_VALUE = 0x80000000;
	static const int MAX_VALUE = 0x7FFFFFFF;

	JSConstraintNode(unsigned _id) {
		localID = _id;
	}

	virtual ~JSConstraintNode() {
	}

	virtual void update(std::vector<nanojit::LIns*> *removables) = 0;
	virtual std::string opcode() = 0;
	virtual std::string toDot() = 0;
	virtual bool needsOvfTest() = 0;
	virtual std::string toString() = 0;

	unsigned id() {
		return localID;
	}

	void addOvsLIns(nanojit::LIns* guard) {
		ovs.add(guard);
	}

};

class JSConstraintVarNode: public JSConstraintNode {

private:
	int var;
	int low;
	int up;

public:
	JSConstraintVarNode* firstAlias;

	JSConstraintVarNode(unsigned _id, int _var, int _low, int _up) :
		JSConstraintNode(_id) {
		var = _var;
		//TODO:Check In Line
		setLimits(_low, _up);
	}

	void update(std::vector<nanojit::LIns*> *removables) {
	}

	void setLimits(int _low, int _up) {
		low = _low;
		up = _up;
	}

	std::string toString();
	std::string toDot();
	std::string opcode();

	int getLowerLimit() {
		return low;
	}
	int getUpperLimit() {
		return up;
	}
	bool needsOvfTest() {
		return false;
	}
	void markOvf() {
	}
};

class JSConstraintCondNode: public JSConstraintNode {

protected:
	JSConstraintVarNode *in1, *in2, *out1, *out2;

public:

	bool needsOvfTest() {
		return false;
	}

	void markOvf() {
	}

	JSConstraintCondNode(unsigned _id, JSConstraintVarNode *_in1,
			JSConstraintVarNode *_in2, JSConstraintVarNode *_out1,
			JSConstraintVarNode *_out2) :
		JSConstraintNode(_id) {
		in1 = _in1;
		in2 = _in2;
		out1 = _out1;
		out2 = _out2;
	}
	virtual ~JSConstraintCondNode() {
	}
	virtual std::string opcode() = 0;
	virtual void update(std::vector<nanojit::LIns*> *removables) = 0;
	std::string toString();
	std::string toDot();
};

class JSConstraintBinOpNode: public JSConstraintNode {

private:
	bool needsOvf;

protected:
	JSConstraintVarNode *in1, *in2, *out;

public:

	JSConstraintBinOpNode(unsigned _id, JSConstraintVarNode *_in1,
			JSConstraintVarNode *_in2, JSConstraintVarNode *_out,
			bool _needsOvf) :
		JSConstraintNode(_id) {
		in1 = _in1;
		in2 = _in2;
		out = _out;
		needsOvf = _needsOvf;

	}
	virtual ~JSConstraintBinOpNode() {
	}

	bool needsOvfTest() {
		return needsOvf;
	}

	void markOvf() {
		needsOvf = true;
	}
	virtual void update(std::vector<nanojit::LIns*> *removables) = 0;
	virtual std::string opcode() = 0;
	std::string toString();
	std::string toDot();

};

class JSConstraintUnOpNode: public JSConstraintNode {

private:

	bool needsOvf;

protected:
	JSConstraintVarNode *use, *def;

public:

	JSConstraintUnOpNode(unsigned _id, JSConstraintVarNode *_def,
			JSConstraintVarNode *_use, bool _needsOvf) :
		JSConstraintNode(_id) {
		use = _use;
		def = _def;
		needsOvf = _needsOvf;
		;
	}
	virtual ~JSConstraintUnOpNode() {
	}
	bool needsOvfTest() {
		return needsOvf;
	}

	void markOvf() {
		needsOvf = true;
	}

	virtual void update(std::vector<nanojit::LIns*> *removables) = 0;
	virtual std::string opcode() = 0;
	std::string toString();
	std::string toDot();
};

class JSConstraintDecNode: public JSConstraintUnOpNode {

public:

	JSConstraintDecNode(unsigned _id, JSConstraintVarNode *_def,
			JSConstraintVarNode *_use) :
		JSConstraintUnOpNode(_id, _def, _use, false) {
	}

	std::string opcode() {
		return "--";
	}

	void update(std::vector<nanojit::LIns*> *removables);
};

class JSConstraintEqNode: public JSConstraintCondNode {

public:

	JSConstraintEqNode(unsigned _id, JSConstraintVarNode *_in1,
			JSConstraintVarNode *_in2, JSConstraintVarNode *_out1,
			JSConstraintVarNode *_out2) :
		JSConstraintCondNode(_id, _in1, _in2, _out1, _out2) {
	}

	std::string opcode() {
		return "==";
	}

	void update(std::vector<nanojit::LIns*> *removables);

};

class JSConstraintGeNode: public JSConstraintCondNode {

public:
	JSConstraintGeNode(unsigned _id, JSConstraintVarNode *_in1,
			JSConstraintVarNode *_in2, JSConstraintVarNode *_out1,
			JSConstraintVarNode *_out2) :
		JSConstraintCondNode(_id, _in1, _in2, _out1, _out2) {
	}

	std::string opcode() {
		return ">=";
	}

	void update(std::vector<nanojit::LIns*> *removables);
};

class JSConstraintGtNode: public JSConstraintCondNode {

public:

	JSConstraintGtNode(unsigned _id, JSConstraintVarNode *_in1,
			JSConstraintVarNode *_in2, JSConstraintVarNode *_out1,
			JSConstraintVarNode *_out2) :
		JSConstraintCondNode(_id, _in1, _in2, _out1, _out2) {

	}

	std::string opcode() {
		return ">";
	}

	void update(std::vector<nanojit::LIns*> *removables);
};

class JSConstraintIncNode: public JSConstraintUnOpNode {

public:
	JSConstraintIncNode(unsigned _id, JSConstraintVarNode *_def,
			JSConstraintVarNode *_use) :
		JSConstraintUnOpNode(_id, _def, _use, false) {

	}

	std::string opcode() {
		return "++";
	}

	void update(std::vector<nanojit::LIns*> *removables);
};

class JSConstraintLeNode: public JSConstraintCondNode {

public:
	JSConstraintLeNode(unsigned _id, JSConstraintVarNode *_in1,
			JSConstraintVarNode *_in2, JSConstraintVarNode *_out1,
			JSConstraintVarNode *_out2) :
		JSConstraintCondNode(_id, _in1, _in2, _out1, _out2) {
	}

	std::string opcode() {
		return "<=";
	}

	void update(std::vector<nanojit::LIns*> *removables);

};

class JSConstraintLtNode: public JSConstraintCondNode {

public:
	JSConstraintLtNode(unsigned _id, JSConstraintVarNode *_in1,
			JSConstraintVarNode *_in2, JSConstraintVarNode *_out1,
			JSConstraintVarNode *_out2) :
		JSConstraintCondNode(_id, _in1, _in2, _out1, _out2) {
	}

	std::string opcode() {
		return "<";
	}

	void update(std::vector<nanojit::LIns*> *removables);
};

class JSConstraintMoveNode: public JSConstraintNode {

private:
	JSConstraintVarNode *def, *use;

public:
	JSConstraintMoveNode(unsigned _id, JSConstraintVarNode *_def,
			JSConstraintVarNode *_use) :
		JSConstraintNode(_id) {
		def = _def;
		use = _use;
	}

	std::string opcode() {
		return ":=";
	}

	void update(std::vector<nanojit::LIns*> *removables);
	bool needsOvfTest() {
		return false;
	}

	void markOvf() {
	}

	std::string toDot();

	std::string toString() {
		return "!MOVE!";
	}
};

class JSConstraintMulNode: public JSConstraintBinOpNode {

public:
	JSConstraintMulNode(unsigned _id, JSConstraintVarNode *_in1,
			JSConstraintVarNode *_in2, JSConstraintVarNode *_out) :
		JSConstraintBinOpNode(_id, _in1, _in2, _out, false) {
	}

	std::string opcode() {
		return "*";
	}

	void update(std::vector<nanojit::LIns*> *removables);
};

class JSConstraintSubNode: public JSConstraintBinOpNode {

public:
	JSConstraintSubNode(unsigned _id, JSConstraintVarNode *_in1,
			JSConstraintVarNode *_in2, JSConstraintVarNode *_out) :
		JSConstraintBinOpNode(_id, _in1, _in2, _out, false) {
	}

	std::string opcode() {
		return "-";
	}

	void update(std::vector<nanojit::LIns*> *removables);
};

class JSConstraintAddNode: public JSConstraintBinOpNode {

public:
	JSConstraintAddNode(unsigned _id, JSConstraintVarNode *_in1,
			JSConstraintVarNode *_in2, JSConstraintVarNode *_out) :
		JSConstraintBinOpNode(_id, _in1, _in2, _out, false) {
	}

	std::string opcode() {
		return "+";
	}

	void update(std::vector<nanojit::LIns*> *removables);

};

class JSVarMap {
private:
	static const unsigned LOCALS_BLOCK_SIZE = 512;
	static const unsigned CONSTANTS_BLOCK_SIZE = 128;
	int localsCapacity;
	int constantsCapacity;

	JSConstraintVarNode** locals;
	JSConstraintVarNode** constants;

	std::map<int, JSConstraintVarNode*> map;
	JSConstraintVarNode* cache[1];
	int cacheID[1];

	void incSize(JSConstraintVarNode** &array, int &actualCapacity,
			unsigned blockSize, int var);
	void putCache(int var, JSConstraintVarNode* node);
	JSConstraintVarNode* getCache(int var);

public:
	JSVarMap() {
		//		locals = new JSConstraintVarNode*[LOCALS_BLOCK_SIZE];
		//		constants = new JSConstraintVarNode*[CONSTANTS_BLOCK_SIZE];
		//
		//		localsCapacity = LOCALS_BLOCK_SIZE;
		//		constantsCapacity = CONSTANTS_BLOCK_SIZE;
		//
		//		memset(locals, NULL, LOCALS_BLOCK_SIZE * sizeof(JSConstraintVarNode*));
		//		memset(constants, NULL, CONSTANTS_BLOCK_SIZE
		//				* sizeof(JSConstraintVarNode*));
		cacheID[0] = 0x8000000;
	}
	JSConstraintVarNode* get(int var);
	void set(int var, JSConstraintVarNode* node);
};

/**
 * This class implements a constraint graph. The constraint graph is used to
 * find the ranges of the variables used in the trace.
 *
 * @author Fernando
 *
 */
class JSConstraintGraph {

private:
	/**
	 * Maps variables to the their nodes
	 */
	//std::map<int, JSConstraintVarNode*> map;
	JSVarMap map;

	/**
	 * This is the list of every node that is part of this contraint graph. In
	 * order to guarantee that the graph is topologically ordered, we can create
	 * the nodes in the order in which the variables in the trace are seen.
	 */
	std::vector<JSConstraintNode*> nodeStack;

	JSConstraintNode* lastAdded;

	unsigned nodeID;
	unsigned constID;
	unsigned accessID;

	unsigned getID() {
		return nodeID++;
	}

	/**
	 * Constructor method.
	 */
public:
	JSConstraintGraph() {
		nodeID = 0;
		accessID = 0;
		lastAdded = 0x0;
	}

	/**
	 * This is the constraint propagation algorithm. The algorithm proceeds in two
	 * phases: first we widen the limits of the variables that have been updated
	 * inside the loop. Widening the limits mean to set these limits to [-INF,
	 * +INF]. After that, we proceed to the propagation phase per-se. The nodes
	 * are visited in the order in which they have been inserted, and constraints
	 * from a node are sent to the next one.
	 */
	void propagateConstraints(std::vector<nanojit::LIns*> *removables);

	/**
	 * Return the node that corresponds to a given variable.
	 *
	 * @param var
	 *        the queried variable
	 * @return the node that corresponds to this variable.
	 */
	JSConstraintVarNode* getVarNode(int var);

	/**
	 * This method adds a new variable to the constraint graph. The limits of this
	 * variable will be set to [-INF, +INF]. Each variable is represented, in the
	 * constraint graph, by a name plus a counter. Adding a variable to the graph
	 * will create a new variable node with counter 0, if the variable did not
	 * exist before, or will create a new variable with n + 1, where n was the
	 * counter of the last occurrence of the variable.
	 *
	 * @param var
	 *        the name of the variable that will be created.
	 * @return The node that corresponds to this newly created variable.
	 */
	JSConstraintVarNode* addVarNode(int var);

	/**
	 * This method creates a new variable, using the same approach as
	 * <code>addVarNode</code> to handle counters. The variable will be created
	 * with the limits specified.
	 *
	 * @param var
	 *        The name of the variable that will be created.
	 * @param low
	 *        The lower limit of this variable
	 * @param up
	 *        The upper limit of this variable
	 * @return the node that corresponds to the newly created variable.
	 */
	JSConstraintVarNode* addVarNode(int var, int low, int up);

	/**
	 * This method will add a unary operator to the constraint graph. Unary
	 * operators, in our very simple language, are only increment (++) and
	 * decrement (--).
	 *
	 * @param var
	 *        The name of the variable that is the target of the operator. This
	 *        method will, effectively, create the unary node var(n+1) = op
	 *        var(n), for instance, i2 = i1++;
	 * @param op
	 *        the opcode of the operator. Currently we handle "++" and "--".
	 */
	void addUnaryArithNode(int var, JSOp op);

	/**
	 * This method adds a new binary node to the constraint graph. We handle three
	 * binary operations: addition (+), subtraction (-) and multiplication (*).
	 * The binary node will have the form res(c1) = a(c2) op b(c3), where op is
	 * the operation, and the c's are the counters of each variable. This method
	 * will create a new variable node to represent res(c1).
	 *
	 * @param a
	 *        One of the operands.
	 * @param b
	 *        The other operand.
	 * @param res
	 *        The result of the operation.
	 * @param op
	 *        The opcode of the operation.
	 */
	void addBinaryArithNode(int a, int b, int res, JSOp op);

	/**
	 * This method adds an assignment node to the constraint graph. Assignments
	 * just copy the value of a variable into another variable. Currently we use
	 * the symbol "=" to denote assignments. This method will create nodes in the
	 * form: def(cd) = use(cu), where cd and cu are counters. The method will
	 * create a new node to represent variable def(cd).
	 *
	 * @param defName
	 *        the name of the right hand side variable in the assignment.
	 * @param useName
	 *        the name of the left hand side variable.
	 */
	void addMoveNode(int defName, int useName);

	/**
	 * This method adds a conditional node into the constraint graph. Conditional
	 * nodes have the form (v1 op v2) => (v1', v2'), where v1 and v2 are the
	 * variables in the source program, and v1' and v2' are new variables that we
	 * create. We create these new variables because we can narrow their limits
	 * using the information that we learn from the result of the conditional. For
	 * instance, if we have that (i[-INF, +INF] < N[10, 10]), then we know that i'
	 * will be in the range [-INF, 9].
	 *
	 * @param v1
	 *        One of the operands of the conditional.
	 * @param v2
	 *        The other operand
	 * @param op
	 *        The opcode of the relational operator. Currently we handle ">",
	 *        ">=", "<", "<=" and "==".
	 */
	void addCondNode(int v1, int v2, JSOp op);

	/**
	 * Return a textual representation of this object, which consists of a list of
	 * all the variables and constraint nodes that the graph contains.
	 *
	 * @return a string object.
	 */
	std::string toString();

	/**
	 * Prints the graph into the given file in dot format. Very useful for
	 * debugging.
	 *
	 * @param fileName
	 *        the name of the file where results will be stored.
	 */
	void dot(std::string file);

	void addLIRtoLastNode(nanojit::LIns* guard);
	void markAsUpdated(int var);
};

#endif /* JSCONSTRAINTGRAPH_H_ */
#define JSCONSTRAINTGRAPH_H_

