import mo_yanxi.call_stream;
import std;

int main(){
	mo_yanxi::call_stream<int(int)> transforms;
	transforms.emplace_back([](int value){
		return value + 1;
	});
	transforms << [](int value){
		return value * 2;
	};

	std::println("reset_and_execute(21)");
	transforms(21, [](int result){
		std::println("  result: {}", result);
	});
	std::println("  is_finished: {}", transforms.is_finished());
	std::println("  has_pending_instructions: {}", transforms.has_pending_instructions());

	std::println("continue_execute(21) after finishing");
	transforms.continue_execute(21, [](int result){
		std::println("  result: {}", result);
	});
	std::println("  no result is printed because no instructions are pending");

	std::println("reset_and_execute(10)");
	transforms.reset_and_execute(10, [](int result){
		std::println("  result: {}", result);
	});

	mo_yanxi::call_stream<void()> steps;
	bool throw_once = true;
	steps.emplace_back([]{
		std::println("  step 1");
	});
	steps.emplace_back([&]{
		std::println("  step 2");
		if(throw_once){
			throw_once = false;
			throw std::runtime_error("transient");
		}
	});
	steps.emplace_back([]{
		std::println("  step 3");
	});

	std::println("reset_and_execute() with a transient exception");
	try{
		steps.reset_and_execute();
		std::println("  completed without exception");
	} catch(const std::runtime_error& error){
		std::println("  caught exception: {}", error.what());
	}
	std::println("  is_partially_executed: {}", steps.is_partially_executed());
	std::println("  has_pending_instructions: {}", steps.has_pending_instructions());

	std::println("continue_execute() resumes from the failed instruction");
	steps.continue_execute();
	std::println("  is_finished: {}", steps.is_finished());
}
