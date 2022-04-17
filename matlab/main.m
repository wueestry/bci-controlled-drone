prompt = "Enter file\n";
Name = input(prompt);
data = readtable(Name);

% if more than one trial uncomment the following section

% data1 = data;
% prompt = "Enter 2nd file\n";
% data2 = readtable(input(prompt));
% prompt = "Enter 3rd file\n";
% data3 = readtable(input(prompt));
% prompt = "Enter 4th file\n";
% data4 = readtable(input(prompt));
% prompt = "Enter 5th file\n";
% data5= readtable(input(prompt));
%data = [data1;data2;data3;data4;data5];


figure;
PlotFFT('Channel 1',data.Channel1,1);
PlotFFT('Channel 2',data.Channel2,2);
PlotFFT('Channel 3',data.Channel3,3);
PlotFFT('Channel 4',data.Channel4,4);

figure;
PlotData('Channel 1',data.Channel1,1);
PlotData('Channel 2',data.Channel2,2);
PlotData('Channel 3',data.Channel3,3);
PlotData('Channel 4',data.Channel4,4);


% The following part is for importing data made by the unicornBCI system
% 
% figure;
% PlotFFT('Channel 1',data.EEG1,1);
% PlotFFT('Channel 2',data.EEG2,2);
% PlotFFT('Channel 3',data.EEG3,3);
% PlotFFT('Channel 4',data.EEG4,4);

% figure;
% PlotData('Channel 1',data.EEG1,1);
% PlotData('Channel 2',data.EEG2,2);
% PlotData('Channel 3',data.EEG3,3);
% PlotData('Channel 4',data.EEG4,4);