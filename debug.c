#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "allStruct.h"

#define EMPPATH "Data/employees.txt"
#define CUSPATH "Data/customers.txt"
#define LOANPATH "Data/loanDetails.txt"
#define COUNTERPATH "Data/loanCounter.txt"
#define HISTORYPATH "Data/trans_hist.txt"
#define FEEDPATH "Data/feedback.txt"

int main()
{
    FILE *f1 = fopen(CUSPATH, "rb+");
    FILE *f2 = fopen(EMPPATH, "rb+");
    FILE *f3 = fopen(LOANPATH, "rb+");
    FILE *f4 = fopen(COUNTERPATH, "rb");
    FILE *f5 = fopen(HISTORYPATH, "rb");
    FILE *f6 = fopen(FEEDPATH, "rb");
    
    struct Customer temp;
    struct Employee temp1;
    struct LoanDetails temp2;
    struct Counter ct;
    struct trans_histroy th;
    struct FeedBack fb;


    // ================ Initialize Loan Counter ================ 
    // ct.count = 0;
    // fwrite(&ct, sizeof(ct), 1, f4);
    // fclose(f4);    
    
    // ================ Reading Loan Counter ================ 
    // ct.count = 0;
    // while (fread(&ct, sizeof(ct), 1, f4))
    // {
    //     printf("%d\n", ct.count);
    // }

    fclose(f4);    
    
    printf("================ Customer ==================\n");
    rewind(f1);
    while(fread(&temp, sizeof(temp), 1, f1) > 0)
    {
        printf("Name: %s\n", temp.customerName);
        printf("Account Number: %d\n", temp.accountNumber);
        printf("Balance: %.2f\n", temp.balance);
        printf("Status: %d\n", temp.activeStatus);
        printf("password: %s\n", temp.password);
    }
    fclose(f1);


    printf("================ Employee ==================\n");
    rewind(f2);
    while(fread(&temp1, sizeof(temp1), 1, f2) > 0)
    {
        printf("Emp ID: %d\n", temp1.empID);
        printf("FirstName: %s\n", temp1.firstName);
        printf("LastName: %s\n", temp1.lastName);
        printf("Role: %d\n", temp1.role);
        printf("Password: %s\n", temp1.password);
    }
    fclose(f2);


    printf("================ Loan Details ==================\n");
    rewind(f3);
    while(fread(&temp2, sizeof(temp2), 1, f3) > 0)
    {
        printf("Emp ID: %d\n", temp2.empID);
        printf("Acc No: %d\n", temp2.accountNumber);
        printf("Loan Amount: %d\n", temp2.loanAmount);
        printf("Status: %d\n", temp2.status);
        printf("Loan ID: %d\n", temp2.loanID);
        printf("=======================\n");
    }
    fclose(f3);

    printf("================ Transaction History ==================\n");
    rewind(f5);
    while(fread(&th, sizeof(th), 1, f5) > 0)
    {
        printf("%d\n", th.acc_no);
        printf("%s\n", th.hist);
        printf("=======================\n");        
    }
    fclose(f5);

    printf("\n================ Feedback ==============\n");
    rewind(f6);
    while(fread(&fb, sizeof(fb), 1, f6) > 0)
    {
        printf("%s\n", fb.feedback);
    } 
    fclose(f6);

    return 0;
}