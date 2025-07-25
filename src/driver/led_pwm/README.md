

make
make clean
make test-file
run :

    sudo ./test_led_c
    sudo ./test_led_cpp



cd led_controller
g++ -std=c++17 -o led_test led_fade_test.cpp led_fade_manager.cpp led_pwm_controller.cpp -lpthread
./sudo led_test



어필할 점 :
    debounce 방식 -> 300 ms 간격으로 중간에 호출시 제거 
    led_pwm_controller 인스턴트 하나만 만든다. : 싱글턴 패턴 적용 

