package ;

import haxe.unit.*;
import tests.*;

class RunTests {

	static function main() {
		var runner = new TestRunner();
		runner.add(new Test1());
		runner.add(new Test2());
		runner.add(new Test1());
		runner.run();
		trace(runner.result.toString());
	}

}
