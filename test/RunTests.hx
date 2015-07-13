package ;

import haxe.unit.*;
import tests.*;

class RunTests {

	static function main() {
		var runner = new TestRunner();
		runner.add(new Test1());
		runner.add(new Test2());
		runner.run();
		trace(runner.result.toString());
		if (!runner.result.success) {
			Sys.exit(1);
		}
	}

}
