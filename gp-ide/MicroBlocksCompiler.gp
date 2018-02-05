// MicroBlocksCompiler.gp - A blocks compiler for microBlocks
// John Maloney, April, 2017

defineClass SmallCompiler opcodes trueObj falseObj

method initialize SmallCompiler {
	initOpcodes this
	trueObj = 4
	falseObj = 8
	return this
}

method initOpcodes SmallCompiler {
	// Initialize the opcode dictionary by parsing definitions copied and pasted from interp.h

	defsFromHeaderFile = '
#define halt 0
#define noop 1
#define pushImmediate 2 // true, false, and ints that fit in 24 bits
#define pushBigImmediate 3 // ints that do not fit in 24 bits (and later, floats)
#define pushLiteral 4 // string or array constant from literals frame
#define pushVar 5
#define popVar 6
#define incrementVar 7
#define pushArgCount 8
#define pushArg 9
#define pushLocal 10
#define popLocal 11
#define incrementLocal 12
#define pop 13
#define jmp 14
#define jmpTrue 15
#define jmpFalse 16
#define decrementAndJmp 17
#define callFunction 18
#define returnResult 19
#define waitMicrosOp 20
#define waitMillisOp 21
#define printIt 22
#define stopAll 23
#define add 24
#define subtract 25
#define multiply 26
#define divide 27
#define lessThan 28
#define newArray 29
#define newByteArray 30
#define fillArray 31
#define at 32
#define atPut 33
#define analogReadOp 34
#define analogWriteOp 35
#define digitalReadOp 36
#define digitalWriteOp 37
#define setLEDOp 38
#define microsOp 39
#define millisOp 40
#define peekOp 41
#define pokeOp 42
#define modulo 43
#define lessOrEq 44
#define equal 45
#define greaterOrEq 46
#define greaterThan 47
#define notOp 48
#define sayIt 49
#define analogPinsOp 50
#define digitalPinsOp 51
#define hexToInt 52
#define i2cGet 53
#define i2cSet 54
#define mbDisplay 55 // temporary micro:bit primitives for demos
#define mbDisplayOff 56
#define mbPlot 57
#define mbUnplot 58
#define mbTiltX 59
#define mbTiltY 60
#define mbTiltZ 61
#define mbTemp 62
#define mbButtonA 63
#define mbButtonB 64
#define random 65
'
	opcodes = (dictionary)
	for line (lines defsFromHeaderFile) {
		words = (words line)
		if (and ((count words) > 2) ('#define' == (first words))) {
			atPut opcodes (at words 2) (toInteger (at words 3))
		}
	}
}

// instruction generation: entry point

method instructionsFor SmallCompiler cmdOrReporter {
	// Return a list of instructions for stack of blocks or a reporter.
	// Add a 'halt' if needed and append any literals (e.g. strings) used.

	if (isClass cmdOrReporter 'Command') {
		op = (primName cmdOrReporter)
		if ('whenCondition' == op) {
			result = (instructionsForWhenCondition this cmdOrReporter)
		} ('whenStarted' == op) {
			result = (instructionsForWhenStarted this cmdOrReporter)
			add result (array 'halt' 0)
		} else {
			result = (instructionsForCmdList this cmdOrReporter)
			add result (array 'halt' 0)
		}
	} else {
		result = (instructionsForCmdList this (newReporter 'return' cmdOrReporter))
	}
	if (and
		((count result) == 2)
		(isOneOf (first (first result)) 'halt' 'stopAll')) {
			// In general, just looking at the final instructon isn't enough because
			// it could just be the end of a conditional body that is jumped
			// over; in that case, we need the final halt as the jump target.
			removeLast result // remove the final halt
	}
	appendLiterals this result
	return result
}

// instruction generation: hat blocks

method instructionsForWhenStarted SmallCompiler cmdOrReporter {
	body = (instructionsForCmdList this (nextBlock cmdOrReporter))
	return body
}

method instructionsForWhenCondition SmallCompiler cmdOrReporter {
	condition = (instructionsForExpression this (first (argList cmdOrReporter)))
	body = (instructionsForCmdList this (nextBlock cmdOrReporter))
	result = (list)

	// poll until condition becomes true
	addAll result condition
	add result (array 'jmpFalse' (0 - ((count condition) + 1)))

	addAll result body

	// loop until condition not true
 	addAll result condition
 	add result (array 'jmpTrue' (0 - ((count condition) + 1)))

	add result (array 'jmp' (0 - ((count result) + 1)))
	return result
}

// instruction generation: command lists and control structures

method instructionsForCmdList SmallCompiler cmdList {
	result = (list)
	cmd = cmdList
	while (notNil cmd) {
		addAll result (instructionsForCmd this cmd)
		cmd = (nextBlock cmd)
	}
	return result
}

method instructionsForCmd SmallCompiler cmd {
	result = (list)
	op = (primName cmd)
	args = (argList cmd)
	if (isOneOf op '=' 'local') {
		addAll result (instructionsForExpression this (at args 2))
		add result (array 'popVar' (globalVarIndex this (first args)))
	} ('+=' == op) {
		addAll result (instructionsForExpression this (at args 2))
		add result (array 'incrementVar' (globalVarIndex this (first args)))
	} ('return' == op) {
		if (0 == (count args)) {
			add result (array 'pushImmediate' 1)
		} else {
			addAll result (instructionsForExpression this (at args 1))
		}
		add result (array 'returnResult' 0)
	} ('stopTask' == op) {
		add result (array 'halt' 0)
	} ('forever' == op) {
		return (instructionsForWhile this (join (array true) args))
	} ('if' == op) {
		return (instructionsForIf this args)
	} ('repeat' == op) {
		return (instructionsForRepeat this args)
	} ('waitUntil' == op) {
		return (instructionsForWaitUntil this args)
	} ('while' == op) {
		return (instructionsForWhile this args)
	} else {
		return (primitive this op args true)
	}
	return result
}

method instructionsForIf SmallCompiler args {
	result = (list)
	jumpsToFix = (list)
	i = 1
	while (i < (count args)) {
		finalCase = ((i + 2) >= (count args)) // true if this is the final case in the if statement
		test = (at args i)
		body = (instructionsForCmdList this (at args (i + 1)))
		if (true != test) {
			addAll result (instructionsForExpression this test)
			offset = (count body)
			if (not finalCase) { offset += 1 }
			add result (array 'jmpFalse' offset)
		}
		addAll result body
		if (not finalCase) {
			jumpToEnd = (array 'jmp' (count result)) // jump offset to be fixed later
			add jumpsToFix jumpToEnd
			add result jumpToEnd
		}
		i += 2
	}
	instructionCount = (count result)
	for jumpInstruction jumpsToFix {
		atPut jumpInstruction 2 (instructionCount - ((at jumpInstruction 2) + 1)) // fix jump offset
	}
	return result
}

method instructionsForForever SmallCompiler args {
	result = (instructionsForCmdList this (at args 1))
	add result (array 'jmp' (0 - ((count result) + 1)))
	return result
}

method instructionsForRepeat SmallCompiler args {
	result = (instructionsForExpression this (at args 1)) // loop count
	body = (instructionsForCmdList this (at args 2))
	addAll result body
	add result (array 'decrementAndJmp' (0 - ((count body) + 1)))
	return result
}

method instructionsForWaitUntil SmallCompiler args {
	conditionTest = (instructionsForExpression this (at args 1))
	result = (list)
	addAll result conditionTest
	add result (array 'jmpFalse' (0 - (+ (count conditionTest) 1)))
	return result
}

method instructionsForWhile SmallCompiler args {
	result = (list)
	body = (instructionsForCmdList this (at args 2))
	if (true == (at args 1)) { // special case: the condition is constant 'true'
		addAll result body
		add result (array 'jmp' (0 - (+ (count body) 1)))
		return result
	}
	conditionTest = (instructionsForExpression this (at args 1))
	add result (array 'jmp' (count body))
	addAll result body
	addAll result conditionTest
	add result (array 'jmpTrue' (0 - (+ (count body) (count conditionTest) 1)))
	return result
}

// instruction generation: expressions

method instructionsForExpression SmallCompiler expr {
	// immediate values
	if (true == expr) {
		return (list (array 'pushImmediate' trueObj))
	} (false == expr) {
		return (list (array 'pushImmediate' falseObj))
	} (isNil expr) {
		return (list (array 'pushImmediate' 1)) // the integer zero
	} (isClass expr 'Integer') {
		if (and (-4194304 <= expr) (expr < 4194303)) { // 23-bit encoded as 24 bit int object
			return (list (array 'pushImmediate' (((expr << 1) | 1) & (hex 'FFFFFF')) ))
		} else {
			return (list
				(array 'pushBigImmediate' 0)
				((expr << 1) | 1)) // 32-bit integer objects follows pushBigImmediate instruction
		}
	} (isClass expr 'String') {
		return (list (array 'pushLiteral' expr))
	} (isClass expr 'Float') {
		error 'Floats are not yet supported'
	}

	// expressions
	op = (primName expr)
	args = (argList expr)
	if ('v' == op) { // variable
		return (list (array 'pushVar' (globalVarIndex this (first args))))
	} ('booleanConstant' == op) {
		if (first args) {
			return (list (array 'pushImmediate' trueObj))
		} else {
			return (list (array 'pushImmediate' falseObj))
		}
	} ('and' == op) {
		return (instructionsForAnd this args)
	} ('or' == op) {
		return (instructionsForOr this args)
	} else {
		return (primitive this op args false)
	}
}

method instructionsForAnd SmallCompiler args {
	tests = (list)
	totalInstrCount = 3 // final three instructions
	for expr args {
		instrList = (instructionsForExpression this expr)
		add tests instrList
		totalInstrCount += ((count instrList) + 1)
	}
	instrCount = 0
	result = (list)
	for t tests {
		addAll result t
		add result (array 'jmpFalse' (totalInstrCount - ((count result) + 2)))
	}
	add result (array 'pushImmediate' trueObj) // all conditions were true: push result
	add result (array 'jmp' 1) // skip over false case
	add result (array 'pushImmediate' falseObj) // some condition was false: push result
	return result
}

method instructionsForOr SmallCompiler args {
	tests = (list)
	totalInstrCount = 3 // final three instructions
	for expr args {
		instrList = (instructionsForExpression this expr)
		add tests instrList
		totalInstrCount += ((count instrList) + 1)
	}
	instrCount = 0
	result = (list)
	for t tests {
		addAll result t
		add result (array 'jmpTrue' (totalInstrCount - ((count result) + 2)))
	}
	add result (array 'pushImmediate' falseObj) // all conditions were false: push result
	add result (array 'jmp' 1) // skip over true case
	add result (array 'pushImmediate' trueObj) // some condition was true: push result
	return result
}

// instruction generation utility methods

method primitive SmallCompiler op args isCommand {
	result = (list)
	if ('print' == op) { op = 'printIt' }
	if ('mbDisplay' == op) {
	  if (25 != (count args)) {
		print 'Display block expects 25 boolean arguments'
		return result
	  }
	  shift = 0
	  displayWord = 0
	  for bit args {
		if (true == bit) { displayWord = (displayWord | (1 << shift)) }
		shift += 1
	  }
	  addAll result (instructionsForExpression this displayWord)
	  add result (array 'mbDisplay' 1)
	  add result (array 'pop' 1)
	} ('comment' == op) {
		// ignore comments
	} (contains opcodes op) {
		for arg args {
			addAll result (instructionsForExpression this arg)
		}
		add result (array op (count args))
		if (and isCommand (not (isOneOf op 'noop' 'stopTask' 'stopAll')))  {
			add result (array 'pop' 1)
		}
	} else {
		print 'Skipping unknown op:' op
	}
	return result
}

method globalVarIndex SmallCompiler varName {
	varNames = (copyWithout (variableNames (targetModule (scripter (smallRuntime)))) 'extensions')
	id = (indexOf varNames varName)
	if (isNil id) {
		error 'Unknown variable' varName
	}
	return (id - 1) // VM uses zero-based index
}

// literal values (strings and large integers )

method appendLiterals SmallCompiler instructions {
	// For now, strings and integers too large for pushImmediate are the only literals.
	// Perhaps add support for constant literal arrays later.

	literals = (list)
	literalOffsets = (dictionary)
	nextOffset = (count instructions)
	for ip (count instructions) {
		instr = (at instructions ip)
		if (and (isClass instr 'Array') ('pushLiteral' == (first instr))) {
			literal = (at instr 2)
			litOffset = (at literalOffsets literal)
			if (isNil litOffset) {
				litOffset = nextOffset
				add literals literal
				atPut literalOffsets literal litOffset
				nextOffset += (wordsForLiteral this literal)
			}
			atPut instr 2 (litOffset - ip)
		}
	}
	addAll instructions literals
}

method wordsForLiteral SmallCompiler literal {
	headerWords = 1
	if (isClass literal 'String') {
		return (headerWords + (floor (((byteCount literal) + 4) / 4)))
	}
	error 'Illegal literal type:' literal
}

// binary code generation

method addBytesForInstructionTo SmallCompiler instr bytes {
	// Append the bytes for the given instruction to bytes (little endian).

	add bytes (at opcodes (first instr))
	arg = (at instr 2)
	if (not (and (-16777216 <= arg) (arg <= 16777215))) {
		error 'Argument does not fit in 24 bits'
	}
	add bytes (arg & 255)
	add bytes ((arg >> 8) & 255)
	add bytes ((arg >> 16) & 255)
}

method addBytesForIntegerLiteralTo SmallCompiler n bytes {
	// Append the bytes for the given integer to bytes (little endian).

	add bytes (n & 255)
	add bytes ((n >> 8) & 255)
	add bytes ((n >> 16) & 255)
	add bytes ((n >> 24) & 255)
}

method addBytesForStringLiteral SmallCompiler s bytes {
	// Append the bytes for the given string to bytes.

	byteCount = (byteCount s)
	wordCount = (floor ((byteCount + 4) / 4))
	headerWord = ((wordCount << 4) | 5);
	repeat 4 { // add header bytes, little endian
		add bytes (headerWord & 255)
		headerWord = (headerWord >> 8)
	}
	for i byteCount {
		add bytes (byteAt s i)
	}
	repeat (4 - (byteCount % 4)) { // pad with zeros to next word boundary
		add bytes 0
	}
}