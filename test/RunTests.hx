package ;

import haxe.unit.TestRunner;

class RunTests {

	static function main() {
		var runner = new TestRunner();
		runner.add(new tests.Test1());
		runner.add(new tests.Test2());
		runner.run();
		trace(runner.result.toString());
		if (!runner.result.success) {
			Sys.exit(1);
		}
	}

}
