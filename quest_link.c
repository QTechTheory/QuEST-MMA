/* @file quest_link.c
 * This file contains the C code for bridging QuEST to Mathematica.
 * While this file contains most of the necessary C code, there are some 
 * minor changes to the QuEST backend - overriding QuEST_validation's 
 * exitWithError, removing some validation, and adding an isCreated field 
 * to the Qureg struct.
 * 
 * The functions defined in this file are grouped as either 'local', 
 * 'callable', 'wrapper', 'internal':
 * - 'local' functions are called from only inside this file.
 * - 'callable' functions are directly callable by a user in MMA.
 * - 'wrapper' functions wrap a specific QuEST function and are directly 
 *    callable by a user in MMA.
 * - 'internal' functions are callable by the MMA, but shouldn't be called
 *    directly by a user - instead, they are wrapped by MMA-defined functions
 * 
 * Note the actual function names called from the MMA code differ from those 
 * below, as specified in quest_templates.tm
 */


#include "wstp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <QuEST.h>

/*
 * PI constant needed for (multiControlled) sGate and tGate
 */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * Codes for specifiying circuits from MMA
 */
#define OPCODE_H 0
#define OPCODE_X 1
#define OPCODE_Y 2
#define OPCODE_Z 3
#define OPCODE_Rx 4
#define OPCODE_Ry 5
#define OPCODE_Rz 6
#define OPCODE_R 7
#define OPCODE_S 8
#define OPCODE_T 9
#define OPCODE_U 10
#define OPCODE_Deph 11
#define OPCODE_Depol 12
#define OPCODE_Damp 13
#define OPCODE_SWAP 14
#define OPCODE_M 15
#define OPCODE_P 16
#define OPCODE_Kraus 17

/**
 * Max number of quregs which can simultaneously exist
 */
#define MAX_NUM_QUREGS 1000

/**
 * Max number of target and control qubits which can be specified 
 * for an individual gate 
 */
#define MAX_NUM_TARGS_CTRLS 100

/**
 * Global instance of QuESTEnv, created when MMA is linked.
 */
QuESTEnv env;

/**
 * Collection of instantiated Quregs
 */
Qureg quregs[MAX_NUM_QUREGS];

/**
 * Buffer for creating error messages
 */
static char buffer[1000];

/**
 * Reports an error message to MMA without aborting
 */
void local_sendErrorToMMA(char* err_msg) {
    WSPutFunction(stdlink, "EvaluatePacket", 1);
    WSPutFunction(stdlink, "Echo", 2);
    WSPutString(stdlink, err_msg);
    WSPutString(stdlink, "Error: ");
    WSEndPacket(stdlink);
    WSNextPacket(stdlink);
    WSNewPacket(stdlink);
}

void local_quregNotCreatedError(int id) {
    sprintf(buffer, "qureg (with id %d) has not been created.", id);
    local_sendErrorToMMA(buffer);
}


/* qureg allocation */

int local_getNextQuregID() {
    for (int id=0; id < MAX_NUM_QUREGS; id++)
        if (!quregs[id].isCreated)
            return id;
    
    local_sendErrorToMMA("Maximum number of quregs have been allocated!");
    return -1;
}

int wrapper_createQureg(int numQubits) {
    int id = local_getNextQuregID();
    quregs[id] = createQureg(numQubits, env);
    quregs[id].isCreated = 1;
    return id;
}
int wrapper_createDensityQureg(int numQubits) {
    int id = local_getNextQuregID();
    quregs[id] = createDensityQureg(numQubits, env);
    quregs[id].isCreated = 1;
    return id;
}
int wrapper_destroyQureg(int id) {
    if (quregs[id].isCreated) {
        destroyQureg(quregs[id], env);
        quregs[id].isCreated = 0;
    } else
        local_quregNotCreatedError(id);
    return id;
}


/** initial states */

int wrapper_initZeroState(int id) {
    if (quregs[id].isCreated)
        initZeroState(quregs[id]);
    else
        local_quregNotCreatedError(id);
    return id;
}
int wrapper_initPlusState(int id) {
    if (quregs[id].isCreated)
        initPlusState(quregs[id]);
    else
        local_quregNotCreatedError(id);
    return id;
}
int wrapper_initClassicalState(int id, int stateInd) {
    if (quregs[id].isCreated)
        initClassicalState(quregs[id], stateInd);
    else
        local_quregNotCreatedError(id);
    return id;
}
int wrapper_initPureState(int quregID, int pureID) {
    if (quregs[quregID].isCreated)
        initPureState(quregs[quregID], quregs[pureID]);
    else
        local_quregNotCreatedError(quregID);
    return quregID;
}
int wrapper_initStateFromAmps(int quregID, qreal* reals, int l1, qreal* imags, int l2) {
    Qureg qureg = quregs[quregID];
    
    if (!qureg.isCreated)
        local_quregNotCreatedError(quregID);
    else if (l1 != l2 || l1 != qureg.numAmpsTotal)
        local_sendErrorToMMA("incorrect number of amplitudes supplied! State has not been changed.");
    else
        initStateFromAmps(qureg, reals, imags);
    return quregID;
}
int wrapper_cloneQureg(int outID, int inID) {
    if (!quregs[outID].isCreated)
        local_quregNotCreatedError(outID);
    else if (!quregs[inID].isCreated)
        local_quregNotCreatedError(inID);
    else
        cloneQureg(quregs[outID], quregs[inID]);
    return outID;
}





/** noise */

int wrapper_applyOneQubitDephaseError(int id, int qb1, qreal prob) {
    if (quregs[id].isCreated)
        applyOneQubitDephaseError(quregs[id], qb1, prob);
    else
        local_quregNotCreatedError(id);
    return id;
}
int wrapper_applyTwoQubitDephaseError(int id, int qb1, int qb2, qreal prob) {
    if (quregs[id].isCreated)
        applyTwoQubitDephaseError(quregs[id], qb1, qb2, prob);
    else
        local_quregNotCreatedError(id);
    return id;
}
int wrapper_applyOneQubitDepolariseError(int id, int qb1, qreal prob) {
    if (quregs[id].isCreated)
        applyOneQubitDepolariseError(quregs[id], qb1, prob);
    else
        local_quregNotCreatedError(id);
    return id;
}
int wrapper_applyTwoQubitDepolariseError(int id, int qb1, int qb2, qreal prob) {
    if (quregs[id].isCreated)
        applyTwoQubitDepolariseError(quregs[id], qb1, qb2, prob);
    else
        local_quregNotCreatedError(id);
    return id;
}
int wrapper_applyOneQubitDampingError(int id, int qb, qreal prob) {
    if (quregs[id].isCreated)
        applyOneQubitDampingError(quregs[id], qb, prob);
    else
        local_quregNotCreatedError(id);
    return id;
}


/* calculations */

qreal wrapper_calcProbOfOutcome(int id, int qb, int outcome) {
    if (quregs[id].isCreated)
        return calcProbOfOutcome(quregs[id], qb, outcome);
    else {
        local_quregNotCreatedError(id);
        return -1;
    }
}
qreal wrapper_calcFidelity(int id1, int id2) {
    if (!quregs[id1].isCreated) {
        local_quregNotCreatedError(id1);
        return -1;
    }
    if (!quregs[id2].isCreated) {
        local_quregNotCreatedError(id2);
        return -1;
    }
    return calcFidelity(quregs[id1], quregs[id2]);
}
void wrapper_calcInnerProduct(int id1, int id2) {
    if (!quregs[id1].isCreated)
        return local_quregNotCreatedError(id1);
    if (!quregs[id2].isCreated)
        return local_quregNotCreatedError(id2);
        
    Complex res = calcInnerProduct(quregs[id1], quregs[id2]);
    WSPutFunction(stdlink, "Complex", 2);
    WSPutReal64(stdlink, res.real);
    WSPutReal64(stdlink, res.imag);
}
qreal wrapper_calcPurity(int id) {
    if (!quregs[id].isCreated) {
        local_quregNotCreatedError(id);
        return -1;
    }
    return calcPurity(quregs[id]);
}
qreal wrapper_calcTotalProb(int id) {
    if (!quregs[id].isCreated) {
        local_quregNotCreatedError(id);
        return -1;
    }
    return calcTotalProb(quregs[id]);
}
qreal wrapper_calcHilbertSchmidtDistance(int id1, int id2) {
    if (!quregs[id1].isCreated) {
        local_quregNotCreatedError(id1);
        return -1;
    }
    if (!quregs[id2].isCreated) {
        local_quregNotCreatedError(id2);
        return -1;
    }
    return calcHilbertSchmidtDistance(quregs[id1], quregs[id2]);
}


/* other modifications */

int wrapper_collapseToOutcome(int id, int qb, int outcome) {
    if (quregs[id].isCreated) {
        collapseToOutcome(quregs[id], qb, outcome);
        return id;
    } else {
        local_quregNotCreatedError(id);
        return -1;
    }
}


/* circuit execution */

void local_backupQuregThenError(char* err_msg, int id, Qureg backup, int* mesOutcomeCache) {
    local_sendErrorToMMA(err_msg);
    cloneQureg(quregs[id], backup);
    destroyQureg(backup, env);
    free(mesOutcomeCache);
    WSPutFunction(stdlink, "Abort", 0);
}

void local_gateUnsupportedError(char* gate, int id, Qureg backup, int* mesOutcomeCache) {
    sprintf(buffer, 
        "the gate '%s' is not supported. "
        "Aborting circuit and restoring qureg (id %d) to its original state.", 
        gate, id);
    local_backupQuregThenError(buffer, id, backup, mesOutcomeCache);
}

void local_gateWrongNumParamsError(char* gate, int wrongNumParams, int rightNumParams, int id, Qureg backup, int* mesOutcomeCache) {
    sprintf(buffer,
        "the gate '%s' accepts %d parameters, but %d were passed. "
        "Aborting circuit and restoring qureg (id %d) to its original state.",
        gate, rightNumParams, wrongNumParams, id);
    local_backupQuregThenError(buffer, id, backup, mesOutcomeCache);
}

/* rightNumTargs is a string so that it can be multiple e.g. "1 or 2" */
void local_gateWrongNumTargsError(char* gate, int wrongNumTargs, char* rightNumTargs, int id, Qureg backup, int* mesOutcomeCache) {
    sprintf(buffer,
        "the gate '%s' accepts %s, but %d were passed. "
        "Aborting circuit and restoring qureg (id %d) to its original state.",
        gate, rightNumTargs, wrongNumTargs, id);
    local_backupQuregThenError(buffer, id, backup, mesOutcomeCache);
}

qreal local_getMaxValidNoiseProb(int opcode, int numQubits) {
    if (opcode == OPCODE_Damp)
        return 1.0;
    if (opcode == OPCODE_Deph) {
        if (numQubits == 1)
            return 1.0/2.0;
        if (numQubits == 2)
            return 3.0/4.0;
    }
    if (opcode == OPCODE_Depol) {
        if (numQubits == 1)
            return 3.0/4.0;
        if (numQubits == 2)
            return 15.0/16.0;
    }
    return -1;
}

int local_isValidProb(int opcode, int numQubits, qreal prob) {
    return (prob > 0 && prob < local_getMaxValidNoiseProb(opcode, numQubits));
}

void local_noiseInvalidProbError(int opcode, int numQubits, qreal prob, int id, Qureg backup, int* mesOutcomeCache) {
    sprintf(buffer, "%d-qubit ", numQubits);
    switch (opcode) {
        case OPCODE_Deph: sprintf(buffer+strlen(buffer), "dephasing"); break;
        case OPCODE_Depol: sprintf(buffer+strlen(buffer), "depolarising"); break;
        case OPCODE_Damp: sprintf(buffer+strlen(buffer), "amplitude damping"); break;
    }
    sprintf(buffer+strlen(buffer),
        " was applied with probability %g which is outside its accepted range of [0, %g]. "
        "Aborting circuit and restoring qureg (id %d) to its original state.",
        prob, local_getMaxValidNoiseProb(opcode, numQubits), id);
    local_backupQuregThenError(buffer, id, backup, mesOutcomeCache);
}

int* local_prepareCtrlCache(int* ctrls, int ctrlInd, int numCtrls, int addTarg) {
    static int ctrlCache[MAX_NUM_TARGS_CTRLS]; 
    for (int i=0; i < numCtrls; i++)
        ctrlCache[i] = ctrls[ctrlInd + i];
    if (addTarg != -1)
        ctrlCache[numCtrls] = addTarg;
    return ctrlCache;
}

/** 
 * Applies a given circuit to the identified qureg.
 * The circuit is expressed as lists of opcodes (identifying gates),
 * the total flat sequence control qubits, a list denoting how many of
 * the control qubits apply to each operation, their target qubits (flat list),
 * a list denotating how many targets each operation has, their parameters 
 * (flat list) and a list denoting how many params each operation has.
 * Returns a list of measurement outcome of any performed measurements in the circuit.
 * The original qureg of the state is restored when this function
 * is aborted by the calling MMA, or aborted due to encountering
 * an invalid gate. In this case, Abort[] is returned.
 * However, a user error caught by the QuEST backend
 * (e.g. same target and control qubit) will result in the link being
 * destroyed.
 */
void internal_applyCircuit(int id) {
    
    // get arguments from MMA link
    int numOps;
    int *opcodes;
    WSGetInteger32List(stdlink, &opcodes, &numOps);
    int totalNumCtrls;
    int *ctrls;
    int *numCtrlsPerOp;
    WSGetInteger32List(stdlink, &ctrls, &totalNumCtrls);
    WSGetInteger32List(stdlink, &numCtrlsPerOp, &numOps);
    int totalNumTargs;
    int *targs;
    int *numTargsPerOp;
    WSGetInteger32List(stdlink, &targs, &totalNumTargs);
    WSGetInteger32List(stdlink, &numTargsPerOp, &numOps);
    int totalNumParams;
    qreal *params;
    int *numParamsPerOp;
    WSGetReal64List(stdlink, &params, &totalNumParams);
    WSGetInteger32List(stdlink, &numParamsPerOp, &numOps);
    
    // ensure qureg exists
    Qureg qureg = quregs[id];
    if (!qureg.isCreated) {
        local_quregNotCreatedError(id);
        WSPutFunction(stdlink, "Abort", 0);
        return;
    }
    
    // backup of initial state in case of abort
    Qureg backup;
    if (qureg.isDensityMatrix)
        backup = createDensityQureg(qureg.numQubitsRepresented, env);
    else
        backup = createQureg(qureg.numQubitsRepresented, env);
    cloneQureg(backup, qureg);
    
    // count the total number of measurements performed in a circuit
    int totalNumMesGates = 0;
    int totalNumMeasurements = 0;
    for (int opInd=0; opInd < numOps; opInd++)
        if (opcodes[opInd] == OPCODE_M) {
            totalNumMesGates++;
            totalNumMeasurements += numTargsPerOp[opInd];
        }
    int* mesOutcomeCache = malloc(totalNumMeasurements * sizeof(int));
    int mesInd = 0;
    
    int ctrlInd = 0;
    int targInd = 0;
    int paramInd = 0;
    
    // attempt to apply each gate
    for (int opInd=0; opInd < numOps; opInd++) {
        
        // check whether the user has tried to abort
        //if (WSAbort) { // why does WSAbort not exist??
        if (WSMessageReady(stdlink)) {
            int code, arg;
            WSGetMessage(stdlink, &code, &arg);
            if (code == WSTerminateMessage || 
                code == WSInterruptMessage || 
                code == WSAbortMessage ||
                code == WSImDyingMessage) {
                    
                return local_backupQuregThenError(
                    "Circuit simulation aborted: restoring original qureg state.",
                    id, backup, mesOutcomeCache);
            }
        }

        // get gate info
        int op = opcodes[opInd];
        int numCtrls = numCtrlsPerOp[opInd];
        int numTargs = numTargsPerOp[opInd];
        int numParams = numParamsPerOp[opInd];
        
        switch(op) {
            
            case OPCODE_H :
                if (numParams != 0)
                    return local_gateWrongNumParamsError("Hadamard", numParams, 0, id, backup, mesOutcomeCache);
                if (numCtrls != 0)
                    return local_gateUnsupportedError("controlled Hadamard", id, backup, mesOutcomeCache);
                if (numTargs != 1)
                    return local_gateWrongNumTargsError("Hadamard", numTargs, "1 target", id, backup, mesOutcomeCache);
                hadamard(qureg, targs[targInd]);
                break;
                
            case OPCODE_S :
                if (numParams != 0)
                    return local_gateWrongNumParamsError("S gate", numParams, 0, id, backup, mesOutcomeCache);
                if (numTargs != 1)
                    return local_gateWrongNumTargsError("S gate", numTargs, "1 target", id, backup, mesOutcomeCache);
                if (numCtrls == 0)
                    sGate(qureg, targs[targInd]);
                else {
                    int* ctrlCache = local_prepareCtrlCache(ctrls, ctrlInd, numCtrls, targs[targInd]);
                    multiControlledPhaseShift(qureg, ctrlCache, numCtrls+1, M_PI/2);
                }
                break;
                
            case OPCODE_T :
                if (numParams != 0)
                    return local_gateWrongNumParamsError("T gate", numParams, 0, id, backup, mesOutcomeCache);
                if (numTargs != 1)
                    return local_gateWrongNumTargsError("T gate", numTargs, "1 target", id, backup, mesOutcomeCache);
                if (numCtrls == 0)
                    tGate(qureg, targs[targInd]);
                else {
                    int* ctrlCache = local_prepareCtrlCache(ctrls, ctrlInd, numCtrls, targs[targInd]);
                    multiControlledPhaseShift(qureg, ctrlCache, numCtrls+1, M_PI/4);
                }
                break;
        
            case OPCODE_X :
                if (numParams != 0)
                    return local_gateWrongNumParamsError("X", numParams, 0, id, backup, mesOutcomeCache);
                if (numTargs != 1)
                    return local_gateWrongNumTargsError("X", numTargs, "1 target", id, backup, mesOutcomeCache);
                if (numCtrls == 0)
                    pauliX(qureg, targs[targInd]);
                else if (numCtrls == 1)
                    controlledNot(qureg, ctrls[ctrlInd], targs[targInd]);
                else {
                    ComplexMatrix2 u = {
                        .r0c0 = {.real=0, .imag=0},
                        .r0c1 = {.real=1, .imag=0},
                        .r1c0 = {.real=1, .imag=0},
                        .r1c1 = {.real=0, .imag=0}};
                    multiControlledUnitary(qureg, &ctrls[ctrlInd], numCtrls, targs[targInd], u);
                }
                break;
                
            case OPCODE_Y :
                if (numParams != 0)
                    return local_gateWrongNumParamsError("Y", numParams, 0, id, backup, mesOutcomeCache);
                if (numTargs != 1)
                    return local_gateWrongNumTargsError("Y", numTargs, "1 target", id, backup, mesOutcomeCache);
                if (numCtrls == 0)
                    pauliY(qureg, targs[targInd]);
                else if (numCtrls == 1)
                    controlledPauliY(qureg, ctrls[ctrlInd], targs[targInd]);
                else
                    return local_gateUnsupportedError("controlled Y", id, backup, mesOutcomeCache);
                break;
                
            case OPCODE_Z :
                if (numParams != 0)
                    return local_gateWrongNumParamsError("Z", numParams, 0, id, backup, mesOutcomeCache);
                if (numTargs != 1)
                    return local_gateWrongNumTargsError("Z", numTargs, "1 target", id, backup, mesOutcomeCache);
                if (numCtrls == 0)
                    pauliZ(qureg, targs[targInd]);
                else {
                    int* ctrlCache = local_prepareCtrlCache(ctrls, ctrlInd, numCtrls, targs[targInd]);
                    multiControlledPhaseFlip(qureg, ctrlCache, numCtrls+1);
                }
                break;
        
            case OPCODE_Rx :
                if (numParams != 1)
                    return local_gateWrongNumParamsError("Rx", numParams, 1, id, backup, mesOutcomeCache);
                if (numTargs != 1)
                    return local_gateWrongNumTargsError("Rx", numTargs, "1 target", id, backup, mesOutcomeCache);
                if (numCtrls == 0)
                    rotateX(qureg, targs[targInd], params[paramInd]);
                else if (numCtrls == 1)
                    controlledRotateX(qureg, ctrls[ctrlInd], targs[targInd], params[paramInd]);
                else
                    return local_gateUnsupportedError("multi-controlled Rotate X", id, backup, mesOutcomeCache);
                break;
                
            case OPCODE_Ry :
                if (numParams != 1)
                    return local_gateWrongNumParamsError("Ry", numParams, 1, id, backup, mesOutcomeCache);
                if (numTargs != 1)
                    return local_gateWrongNumTargsError("Ry", numTargs, "1 target", id, backup, mesOutcomeCache);
                if (numCtrls == 0)
                    rotateY(qureg, targs[targInd], params[paramInd]);
                else if (numCtrls == 1)
                    controlledRotateY(qureg, ctrls[ctrlInd], targs[targInd], params[paramInd]);
                else
                    return local_gateUnsupportedError("multi-controlled Rotate Y", id, backup, mesOutcomeCache);
                break;
                
            case OPCODE_Rz :
                if (numParams != 1)
                    return local_gateWrongNumParamsError("Rz", numParams, 1, id, backup, mesOutcomeCache);
                if (numCtrls > 1)
                    return local_gateUnsupportedError("multi-controlled Rotate Z", id, backup, mesOutcomeCache);
                if (numCtrls == 1 && numTargs > 1)
                    return local_gateUnsupportedError("multi-controlled multi-rotateZ", id, backup, mesOutcomeCache);
                if (numTargs == 1) {
                    if (numCtrls == 0)
                        rotateZ(qureg, targs[targInd], params[paramInd]);
                    if (numCtrls == 1)
                        controlledRotateZ(qureg, ctrls[ctrlInd], targs[targInd], params[paramInd]);
                } else
                    multiRotateZ(qureg, &targs[targInd], numTargs, params[paramInd]);
                break;
                
            case OPCODE_R:
                if (numCtrls != 0)
                    return local_gateUnsupportedError("controlled multi-rotate-Pauli", id, backup, mesOutcomeCache);
                if (numTargs != numParams-1) {
                    sprintf(buffer, 
                        "An internel error in R occured! It received an unequal number of Pauli codes (%d) and target qubits! (%d)",
                        numParams-1, numTargs);
                    return local_backupQuregThenError(buffer, id, backup, mesOutcomeCache);
                }
                int paulis[MAX_NUM_TARGS_CTRLS]; 
                for (int p=0; p < numTargs; p++)
                    paulis[p] = params[paramInd+1+p];
                multiRotatePauli(qureg, &targs[targInd], paulis, numTargs, params[paramInd]);
                break;
            
            case OPCODE_U : 
                if (numTargs == 1 && numParams != 2*2*2)
                    return local_backupQuregThenError("single qubit U accepts only 2x2 matrices", id, backup, mesOutcomeCache);
                if (numTargs == 2 && numParams != 4*4*2)
                    return local_backupQuregThenError("two qubit U accepts only 4x4 matrices", id, backup, mesOutcomeCache);
                if (numTargs != 1 && numTargs != 2)
                    return local_gateWrongNumTargsError("U", numTargs, "1 or 2 targets", id, backup, mesOutcomeCache);
                
                if (numTargs == 1) {
                    ComplexMatrix2 u = {
                        .r0c0={.real=params[paramInd+0], .imag=params[paramInd+1]},
                        .r0c1={.real=params[paramInd+2], .imag=params[paramInd+3]},
                        .r1c0={.real=params[paramInd+4], .imag=params[paramInd+5]},
                        .r1c1={.real=params[paramInd+6], .imag=params[paramInd+7]}};
                    if (numCtrls == 0)
                        unitary(qureg, targs[targInd], u);
                    else
                        multiControlledUnitary(qureg, &ctrls[ctrlInd], numCtrls, targs[targInd], u);
                }
                else if (numTargs == 2) {
                    ComplexMatrix4 u = {
                        .r0c0={.real=params[paramInd+0], .imag=params[paramInd+1]},
                        .r0c1={.real=params[paramInd+2], .imag=params[paramInd+3]},
                        .r0c2={.real=params[paramInd+4], .imag=params[paramInd+5]},
                        .r0c3={.real=params[paramInd+6], .imag=params[paramInd+7]},
                        .r1c0={.real=params[paramInd+8], .imag=params[paramInd+9]},
                        .r1c1={.real=params[paramInd+10], .imag=params[paramInd+11]},
                        .r1c2={.real=params[paramInd+12], .imag=params[paramInd+13]},
                        .r1c3={.real=params[paramInd+14], .imag=params[paramInd+15]},
                        .r2c0={.real=params[paramInd+16], .imag=params[paramInd+17]},
                        .r2c1={.real=params[paramInd+18], .imag=params[paramInd+19]},
                        .r2c2={.real=params[paramInd+20], .imag=params[paramInd+21]},
                        .r2c3={.real=params[paramInd+22], .imag=params[paramInd+23]},
                        .r3c0={.real=params[paramInd+24], .imag=params[paramInd+25]},
                        .r3c1={.real=params[paramInd+26], .imag=params[paramInd+27]},
                        .r3c2={.real=params[paramInd+28], .imag=params[paramInd+29]},
                        .r3c3={.real=params[paramInd+30], .imag=params[paramInd+31]}};
                    if (numCtrls == 0)
                        twoQubitUnitary(qureg, targs[targInd], targs[targInd+1], u);
                    else
                        multiControlledTwoQubitUnitary(qureg, &ctrls[ctrlInd], numCtrls, targs[targInd], targs[targInd+1], u);
                }
                break;
                
            case OPCODE_Deph :
                if (numParams != 1)
                    return local_gateWrongNumParamsError("Dephasing", numParams, 1, id, backup, mesOutcomeCache);
                if (numCtrls != 0)
                    return local_gateUnsupportedError("controlled dephasing", id, backup, mesOutcomeCache);
                if (numTargs != 1 && numTargs != 2)
                    return local_gateWrongNumTargsError("Dephasing", numTargs, "1 or 2 targets", id, backup, mesOutcomeCache);
                if (params[paramInd] == 0)
                    break;
                if (!local_isValidProb(op, numTargs, params[paramInd]))
                    return local_noiseInvalidProbError(op, numTargs, params[paramInd], id, backup, mesOutcomeCache);
                if (numTargs == 1)
                    applyOneQubitDephaseError(qureg, targs[targInd], params[paramInd]);
                if (numTargs == 2)
                    applyTwoQubitDephaseError(qureg, targs[targInd], targs[targInd+1], params[paramInd]);
                break;
                
            case OPCODE_Depol :
                if (numParams != 1)
                    return local_gateWrongNumParamsError("Depolarising", numParams, 1, id, backup, mesOutcomeCache);
                if (numCtrls != 0)
                    return local_gateUnsupportedError("controlled depolarising", id, backup, mesOutcomeCache);
                if (numTargs != 1 && numTargs != 2)
                    return local_gateWrongNumTargsError("Depolarising", numTargs, "1 or 2 targets", id, backup, mesOutcomeCache);
                if (params[paramInd] == 0)
                    break;
                if (!local_isValidProb(op, numTargs, params[paramInd]))
                    return local_noiseInvalidProbError(op, numTargs, params[paramInd], id, backup, mesOutcomeCache);
                if (numTargs == 1)
                    applyOneQubitDepolariseError(qureg, targs[targInd], params[paramInd]);
                if (numTargs == 2)
                    applyTwoQubitDepolariseError(qureg, targs[targInd], targs[targInd+1], params[paramInd]);
                break;
                
            case OPCODE_Damp :
                if (numParams != 1)
                    return local_gateWrongNumParamsError("Damping", numParams, 1, id, backup, mesOutcomeCache);
                if (numCtrls != 0)
                    return local_gateUnsupportedError("controlled damping", id, backup, mesOutcomeCache);
                if (numTargs != 1)
                    return local_gateWrongNumTargsError("Damping", numTargs, "1 target", id, backup, mesOutcomeCache);
                if (params[paramInd] == 0)
                    break;
                if (!local_isValidProb(op, numTargs, params[paramInd]))
                    return local_noiseInvalidProbError(op, numTargs, params[paramInd], id, backup, mesOutcomeCache);
                applyOneQubitDampingError(qureg, targs[targInd], params[paramInd]);
                break;
                
            case OPCODE_SWAP:
                if (numParams != 0)
                    return local_gateWrongNumParamsError("SWAP", numParams, 0, id, backup, mesOutcomeCache);
                if (numTargs != 2)
                    return local_gateWrongNumTargsError("Depolarising", numTargs, "2 targets", id, backup, mesOutcomeCache);
                if (numCtrls == 0) {
                    controlledNot(qureg, targs[targInd],   targs[targInd+1]);
                    controlledNot(qureg, targs[targInd+1], targs[targInd  ]);
                    controlledNot(qureg, targs[targInd],   targs[targInd+1]);
                } else {
                    ComplexMatrix2 u = {
                        .r0c0 = {.real=0, .imag=0},
                        .r0c1 = {.real=1, .imag=0},
                        .r1c0 = {.real=1, .imag=0},
                        .r1c1 = {.real=0, .imag=0}};
                    int* ctrlCache = local_prepareCtrlCache(ctrls, ctrlInd, numCtrls, targs[targInd]);
                    multiControlledUnitary(qureg, ctrlCache, numCtrls+1, targs[targInd+1], u);
                    ctrlCache[numCtrls] = targs[targInd+1];
                    multiControlledUnitary(qureg, ctrlCache, numCtrls+1, targs[targInd], u);
                    ctrlCache[numCtrls] = targs[targInd];
                    multiControlledUnitary(qureg, ctrlCache, numCtrls+1, targs[targInd+1], u);
                }
                break;
                
            case OPCODE_M:
                if (numParams != 0)
                    return local_gateWrongNumParamsError("M", numParams, 0, id, backup, mesOutcomeCache);
                if (numCtrls != 0)
                    return local_gateUnsupportedError("controlled measurement", id, backup, mesOutcomeCache);
                for (int q=0; q < numTargs; q++)
                    mesOutcomeCache[mesInd++] = measure(qureg, targs[targInd+q]);
                break;
            
            case OPCODE_P:
                if (numParams != 1 && numParams != numTargs) {
                    sprintf(buffer, 
                        "P[outcomes] specified a different number of binary outcomes (%d) than target qubits (%d)!",
                        numParams, numTargs);
                    return local_backupQuregThenError(buffer, id, backup, mesOutcomeCache);
                }
                if (numCtrls != 0)
                    return local_gateUnsupportedError("controlled projector", id, backup, mesOutcomeCache);
                if (numParams > 1)
                    for (int q=0; q < numParams; q++)
                        collapseToOutcome(qureg, targs[targInd+q], (int) params[paramInd+q]);
                else {
                    // check value isn't impossibly high
                    if (params[paramInd] >= (1LL << numTargs)) {
                        sprintf(buffer, "P[%d] was applied to %d qubits and exceeds their maximum represented value of %lld.",
                            (int) params[paramInd], numTargs, (1LL << numTargs));
                        return local_backupQuregThenError(buffer, id, backup, mesOutcomeCache);
                    }
                    // work out each bit outcome and apply; right most (least significant) bit acts on right-most target
                    for (int q=0; q < numTargs; q++)
                        collapseToOutcome(qureg, targs[targInd+numTargs-q-1], (((int) params[paramInd]) >> q) & 1);
                }
                break;
                
            case OPCODE_Kraus:
                ; // empty post-label statement, courtesy of weird C99 standard
                int numKrausOps = (int) params[paramInd];
                if (numCtrls != 0)
                    return local_gateUnsupportedError("controlled Kraus map", id, backup, mesOutcomeCache);
                if (numTargs != 1 && numTargs != 2)
                    return local_gateWrongNumTargsError("Kraus map", numTargs, "1 or 2 targets", id, backup, mesOutcomeCache);
                if ((numKrausOps < 1) ||
                    (numTargs == 1 && numKrausOps > 4 ) ||
                    (numTargs == 2 && numKrausOps > 16))
                {
                    sprintf(buffer, 
                        "%d operators were passed to single-qubit Krauss[ops], which accepts only >0 and <=%d operators!",
                        numKrausOps, (numTargs==1)? 4:16);
                    return local_backupQuregThenError(buffer, id, backup, mesOutcomeCache);
                }
                if (numTargs == 1 && (numParams-1) != 2*2*2*numKrausOps)
                    return local_backupQuregThenError("one-qubit Kraus expects 2-by-2 matrices!", id, backup, mesOutcomeCache);
                if (numTargs == 2 && (numParams-1) != 4*4*2*numKrausOps)
                    return local_backupQuregThenError("two-qubit Kraus expects 4-by-4 matrices!", id, backup, mesOutcomeCache);

                if (numTargs == 1) {
                    ComplexMatrix2 krausOps[4];
                    int opElemInd = 1 + paramInd;
                    for (int n=0; n < numKrausOps; n++) {
                        krausOps[n].r0c0.real = params[opElemInd++]; krausOps[n].r0c0.imag = params[opElemInd++];
                        krausOps[n].r0c1.real = params[opElemInd++]; krausOps[n].r0c1.imag = params[opElemInd++];
                        krausOps[n].r1c0.real = params[opElemInd++]; krausOps[n].r1c0.imag = params[opElemInd++];
                        krausOps[n].r1c1.real = params[opElemInd++]; krausOps[n].r1c1.imag = params[opElemInd++];
                    }
                    applyOneQubitKrausMap(qureg, targs[targInd], krausOps, numKrausOps);
                } 
                else if (numTargs == 2) {
                    ComplexMatrix4 krausOps[16];
                    int opElemInd = 1 + paramInd;
                    for (int n=0; n < numKrausOps; n++) {
                        krausOps[n].r0c0.real = params[opElemInd++]; krausOps[n].r0c0.imag = params[opElemInd++];
                        krausOps[n].r0c1.real = params[opElemInd++]; krausOps[n].r0c1.imag = params[opElemInd++];
                        krausOps[n].r0c2.real = params[opElemInd++]; krausOps[n].r0c2.imag = params[opElemInd++];
                        krausOps[n].r0c3.real = params[opElemInd++]; krausOps[n].r0c3.imag = params[opElemInd++];

                        krausOps[n].r1c0.real = params[opElemInd++]; krausOps[n].r1c0.imag = params[opElemInd++];
                        krausOps[n].r1c1.real = params[opElemInd++]; krausOps[n].r1c1.imag = params[opElemInd++];
                        krausOps[n].r1c2.real = params[opElemInd++]; krausOps[n].r1c2.imag = params[opElemInd++];
                        krausOps[n].r1c3.real = params[opElemInd++]; krausOps[n].r1c3.imag = params[opElemInd++];
                        
                        krausOps[n].r2c0.real = params[opElemInd++]; krausOps[n].r2c0.imag = params[opElemInd++];
                        krausOps[n].r2c1.real = params[opElemInd++]; krausOps[n].r2c1.imag = params[opElemInd++];
                        krausOps[n].r2c2.real = params[opElemInd++]; krausOps[n].r2c2.imag = params[opElemInd++];
                        krausOps[n].r2c3.real = params[opElemInd++]; krausOps[n].r2c3.imag = params[opElemInd++];
                        
                        krausOps[n].r3c0.real = params[opElemInd++]; krausOps[n].r3c0.imag = params[opElemInd++];
                        krausOps[n].r3c1.real = params[opElemInd++]; krausOps[n].r3c1.imag = params[opElemInd++];
                        krausOps[n].r3c2.real = params[opElemInd++]; krausOps[n].r3c2.imag = params[opElemInd++];
                        krausOps[n].r3c3.real = params[opElemInd++]; krausOps[n].r3c3.imag = params[opElemInd++];
                    }
                    
                    applyTwoQubitKrausMap(qureg, targs[targInd], targs[targInd+1], krausOps, numKrausOps);
                } 
                break;
                
            default:            
                return local_backupQuregThenError(
                    "circuit contained an unknown gate. "
                    "Aborting circuit and restoring original state.",
                    id, backup, mesOutcomeCache);
        }
        ctrlInd += numCtrls;
        targInd += numTargs;
        paramInd += numParams;
    }
    
    // return lists of measurement outcomes
    mesInd = 0;
    WSPutFunction(stdlink, "List", totalNumMesGates);
    for (int opInd=0; opInd < numOps; opInd++) {
        if (opcodes[opInd] == OPCODE_M) {
            WSPutFunction(stdlink, "List", numTargsPerOp[opInd]);
            for (int i=0; i < numTargsPerOp[opInd]; i++)
                WSPutInteger(stdlink, mesOutcomeCache[mesInd++]);
        }
    }
    
    // clear data structures
    free(mesOutcomeCache);
    destroyQureg(backup, env);
}

/**
 * puts a Qureg into MMA, with the structure of
 * {numQubits, isDensityMatrix, realAmps, imagAmps}.
 * Instead gives -1 if error (e.g. qureg id is wrong)
 */
void internal_getStateVec(int id) {
    
    if (!quregs[id].isCreated) {
        local_quregNotCreatedError(id);
        WSPutInteger(stdlink, -1);
        return;
    }

    Qureg qureg = quregs[id];
    syncQuESTEnv(env);
    copyStateFromGPU(qureg); // does nothing on CPU
    
    WSPutFunction(stdlink, "List", 4);
    WSPutInteger(stdlink, qureg.numQubitsRepresented);
    WSPutInteger(stdlink, qureg.isDensityMatrix);
    WSPutReal64List(stdlink, qureg.stateVec.real, qureg.numAmpsTotal);
    WSPutReal64List(stdlink, qureg.stateVec.imag, qureg.numAmpsTotal);
}

int internal_setWeightedQureg(
    double facRe1, double facIm1, int qureg1, 
    double facRe2, double facIm2, int qureg2, 
    double facReOut, double facImOut, int outID
) {
    if (!quregs[qureg1].isCreated)
        local_quregNotCreatedError(qureg1);
    else if (!quregs[qureg2].isCreated)
        local_quregNotCreatedError(qureg2);
    else if (!quregs[outID].isCreated)
        local_quregNotCreatedError(outID);
    else 
        setWeightedQureg(
            (Complex) {.real=facRe1, .imag=facIm1}, quregs[qureg1],
            (Complex) {.real=facRe2, .imag=facIm2}, quregs[qureg2],
            (Complex) {.real=facReOut, .imag=facImOut}, quregs[outID]);
    return outID;
}


/* Evaluates the expected value of a Pauli product */
qreal internal_calcExpecPauliProd(int quregId, int workspaceId) {
    
    // get arguments from MMA link
    int numPaulis;
    int *pauliCodes;
    WSGetInteger32List(stdlink, &pauliCodes, &numPaulis);
    int *targs;
    WSGetInteger32List(stdlink, &targs, &numPaulis);
    
    // ensure quregs exists
    Qureg qureg = quregs[quregId];
    if (!qureg.isCreated) {
        local_quregNotCreatedError(quregId);
        WSPutFunction(stdlink, "Abort", 0);
        return -1; // @TODO NEEDS FIXING!! -1 stuck in pipeline
    }
    Qureg workspace = quregs[workspaceId];
    if (!workspace.isCreated) {
        local_quregNotCreatedError(workspaceId);
        WSPutFunction(stdlink, "Abort", 0);
        return -1; // @TODO NEEDS FIXING!! -1 stuck in pipeline
    }
    
    return calcExpecPauliProd(qureg, targs, pauliCodes, numPaulis, workspace);
}

void local_loadPauliSumFromMMA(int numQb, int* numTerms, int** arrPaulis, qreal** termCoeffs) {
    
    // get arguments from MMA link
    int numPaulis;
    WSGetReal64List(stdlink, termCoeffs, numTerms);
    int* allPauliCodes;
    WSGetInteger32List(stdlink, &allPauliCodes, &numPaulis);
    int* allPauliTargets;
    WSGetInteger32List(stdlink, &allPauliTargets, &numPaulis);
    int* numPaulisPerTerm;
    WSGetInteger32List(stdlink, &numPaulisPerTerm, numTerms);
    
    // convert {allPauliCodes}, {allPauliTargets}, {numPaulisPerTerm}, and
    // qureg.numQubitsRepresented into {pauli-code-for-every-qubit}
    int arrLen = *numTerms * numQb;
    *arrPaulis = malloc(arrLen * sizeof(int));
    for (int i=0; i < arrLen; i++)
        (*arrPaulis)[i] = 0;
    
    int allPaulisInd = 0;
    for (int t=0;  t < *numTerms; t++) {
        for (int j=0; j < numPaulisPerTerm[t]; j++) {
            int arrInd = t*numQb + allPauliTargets[allPaulisInd];
            (*arrPaulis)[arrInd] = allPauliCodes[allPaulisInd++];
        }
    }
    
    WSReleaseInteger32List(stdlink, allPauliCodes, numPaulis);
    WSReleaseInteger32List(stdlink, allPauliTargets, numPaulis);
    // arrPaulis and termCoeffs must be freed by caller using free and WSDisown respectively
}

qreal internal_calcExpecPauliSum(int quregId, int workspaceId) {
        
    // ensure quregs exists
    Qureg qureg = quregs[quregId];
    if (!qureg.isCreated) {
        local_quregNotCreatedError(quregId);
        WSPutFunction(stdlink, "Abort", 0);
        return -1; // @TODO NEEDS FIXING!! -1 stuck in pipeline
    }
    Qureg workspace = quregs[workspaceId];
    if (!workspace.isCreated) {
        local_quregNotCreatedError(workspaceId);
        WSPutFunction(stdlink, "Abort", 0);
        return -1; // @TODO NEEDS FIXING!! -1 stuck in pipeline
    }
    
    int numTerms; int* arrPaulis; qreal* termCoeffs;
    local_loadPauliSumFromMMA(qureg.numQubitsRepresented, &numTerms, &arrPaulis, &termCoeffs);
    
    qreal val = calcExpecPauliSum(qureg, arrPaulis, termCoeffs, numTerms, workspace);
    
    free(arrPaulis);
    WSReleaseReal64List(stdlink, termCoeffs, numTerms);
    
    return val;
}


void internal_applyPauliSum(int inId, int outId) {

    // ensure quregs exists
    Qureg inQureg = quregs[inId];
    if (!inQureg.isCreated) {
        local_quregNotCreatedError(inId);
        WSPutFunction(stdlink, "Abort", 0);
        return;
    }
    Qureg outQureg = quregs[outId];
    if (!outQureg.isCreated) {
        local_quregNotCreatedError(outId);
        WSPutFunction(stdlink, "Abort", 0);
        return;
    }
    
    int numTerms; int* arrPaulis; qreal* termCoeffs;
    local_loadPauliSumFromMMA(inQureg.numQubitsRepresented, &numTerms, &arrPaulis, &termCoeffs);
    
    applyPauliSum(inQureg, arrPaulis, termCoeffs, numTerms, outQureg);
    
    free(arrPaulis);
    WSReleaseReal64List(stdlink, termCoeffs, numTerms);
}


/**
 * Frees all quregs
 */
int callable_destroyAllQuregs(void) {
    
    for (int id=0; id < MAX_NUM_QUREGS; id++) {
        if (quregs[id].isCreated) {
            destroyQureg(quregs[id], env);
            quregs[id].isCreated = 0;
        }
    }
    return -1;
}

/** 
 * Returns a list of all created quregs
 */
void callable_getAllQuregs(void) {
    
    // collect all created quregs
    int numQuregs = 0;
    int idList[MAX_NUM_QUREGS];
    for (int id=0; id < MAX_NUM_QUREGS; id++)
        if (quregs[id].isCreated)
            idList[numQuregs++] = id;
    
    WSPutIntegerList(stdlink, idList, numQuregs);
}



int main(int argc, char* argv[]) {
    
    // create the single, global QuEST execution env
    env = createQuESTEnv();
    
    // indicate that no quregs have yet been created
    for (int id=0; id < MAX_NUM_QUREGS; id++)
        quregs[id].isCreated = 0;
    
    // establish link with MMA
	return WSMain(argc, argv);
}
