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

	std::vector<int> results;
	transforms(21, [&](int result){
		results.push_back(result);
	});

	if(results.size() != 2 || results[0] != 22 || results[1] != 42){
		return 1;
	}
	if(!transforms.is_finished() || transforms.has_pending_instructions()){
		return 2;
	}

	transforms.continue_execute(21, [&](int result){
		results.push_back(result);
	});
	if(results.size() != 2){
		return 3;
	}

	transforms.reset_and_execute(10, [&](int result){
		results.push_back(result);
	});
	if(results.size() != 4 || results[2] != 11 || results[3] != 20){
		return 4;
	}

	mo_yanxi::call_stream<void()> steps;
	std::vector<int> seen;
	bool throw_once = true;
	steps.emplace_back([&]{
		seen.push_back(1);
	});
	steps.emplace_back([&]{
		seen.push_back(2);
		if(throw_once){
			throw_once = false;
			throw std::runtime_error("transient");
		}
	});
	steps.emplace_back([&]{
		seen.push_back(3);
	});

	try{
		steps.reset_and_execute();
		return 5;
	} catch(const std::runtime_error&){
	}

	if(!steps.is_partially_executed() || !steps.has_pending_instructions()){
		return 6;
	}

	steps.continue_execute();
	if(!steps.is_finished() || seen != std::vector<int>{1, 2, 2, 3}){
		return 7;
	}

	return 0;
}
